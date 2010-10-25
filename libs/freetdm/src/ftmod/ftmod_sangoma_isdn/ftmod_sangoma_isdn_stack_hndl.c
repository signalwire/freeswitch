/*
 * Copyright (c) 2010, Sangoma Technologies
 * David Yat Sin <dyatsin@sangoma.com>
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

extern ftdm_status_t cpy_calling_num_from_stack(ftdm_caller_data_t *ftdm, CgPtyNmb *cgPtyNmb);
extern ftdm_status_t cpy_called_num_from_stack(ftdm_caller_data_t *ftdm, CdPtyNmb *cdPtyNmb);
extern ftdm_status_t cpy_redir_num_from_stack(ftdm_caller_data_t *ftdm, RedirNmb *redirNmb);
extern ftdm_status_t cpy_calling_name_from_stack(ftdm_caller_data_t *ftdm, Display *display);

/* Remote side transmit a SETUP */
void sngisdn_process_con_ind (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	unsigned i;
	int16_t suId = sngisdn_event->suId;
	uint32_t suInstId = sngisdn_event->suInstId;
	uint32_t spInstId = sngisdn_event->spInstId;
	int16_t dChan = sngisdn_event->dChan;
	uint8_t ces = sngisdn_event->ces;
	sngisdn_chan_data_t *sngisdn_info = sngisdn_event->sngisdn_info;	
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;
	ConEvnt *conEvnt = &sngisdn_event->event.conEvnt;

	ftdm_assert(!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE), "State change flag pending\n");
	
	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_DEBUG, "Processing SETUP (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
		
	switch (ftdmchan->state) {
		case FTDM_CHANNEL_STATE_DOWN: /* Proper state to receive a SETUP */
			if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_INUSE) ||
				ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {

				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_INFO, "Received SETUP but channel is in USE, saving call for later processing\n");
				/* the flag the channel as having a collision */
				sngisdn_set_flag(sngisdn_info, FLAG_GLARE);

				/* save the SETUP for processing once the channel has gone to DOWN */
				memcpy(&sngisdn_info->glare.setup, conEvnt, sizeof(*conEvnt));
				sngisdn_info->glare.suId = suId;
				sngisdn_info->glare.suInstId = suInstId; /* Do not generate a suInstId now, we will generate when glared call gets extracted */
				sngisdn_info->glare.spInstId = spInstId;
				sngisdn_info->glare.dChan = dChan;
				sngisdn_info->glare.ces = ces;
				break;
			}
			
			sngisdn_info->suInstId = get_unique_suInstId(suId);
			sngisdn_info->spInstId = spInstId;
			

			if (conEvnt->cdPtyNmb.eh.pres && signal_data->num_local_numbers) {
				uint8_t local_number_matched = 0;
				for (i = 0 ; i < signal_data->num_local_numbers ; i++) {
					if (!strcmp(signal_data->local_numbers[i], (char*)conEvnt->cdPtyNmb.nmbDigits.val)) {
						local_number_matched++;
						break;
					}
				}
				if (!local_number_matched) {
					ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Received SETUP, but local-number %s does not match - ignoring\n", conEvnt->cdPtyNmb.nmbDigits.val);
					/* Special case to tell the stack to clear all internal resources about this call. We will no receive any event for this call after sending disconnect request */
					ftdmchan->caller_data.hangup_cause = IN_CCNORTTODEST;
					ftdm_sched_timer(signal_data->sched, "delayed_disconnect", 1, sngisdn_delayed_disconnect, (void*) sngisdn_info, NULL);
					return;
				}
			}

			/* If this is a glared call that was previously saved, we moved
			all the info to the current call, so clear the glared saved data */
			if (sngisdn_info->glare.spInstId == spInstId) {
				clear_call_glare_data(sngisdn_info);
			}

			
			if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP) {
				if (signal_data->signalling == SNGISDN_SIGNALING_NET) {
					sngisdn_info->ces = ces;
				}
			}

			ftdm_mutex_lock(g_sngisdn_data.ccs[suId].mutex);
			g_sngisdn_data.ccs[suId].active_suInstIds[sngisdn_info->suInstId] = sngisdn_info;
			ftdm_mutex_unlock(g_sngisdn_data.ccs[suId].mutex);

			ftdm_clear_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND);

			/* try to open the channel */
			if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Failed to open channel");
				sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_REL);
				ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_NORMAL_TEMPORARY_FAILURE;
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_CANCEL);
				break;
			}

#if 0
			/* Export ftdmchan variables here if we need to */
			ftdm_channel_add_var(ftdmchan, "isdn_specific_var", "1");
#endif
			/* Fill in call information */
			cpy_calling_num_from_stack(&ftdmchan->caller_data, &conEvnt->cgPtyNmb);
			cpy_called_num_from_stack(&ftdmchan->caller_data, &conEvnt->cdPtyNmb);
			cpy_calling_name_from_stack(&ftdmchan->caller_data, &conEvnt->display);
			ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Incoming call: Called No:[%s] Calling No:[%s]\n", ftdmchan->caller_data.dnis.digits, ftdmchan->caller_data.cid_num.digits);

			if (conEvnt->bearCap[0].eh.pres) {
				ftdmchan->caller_data.bearer_layer1 = sngisdn_get_infoTranCap_from_stack(conEvnt->bearCap[0].usrInfoLyr1Prot.val);
				ftdmchan->caller_data.bearer_capability = sngisdn_get_infoTranCap_from_stack(conEvnt->bearCap[0].infoTranCap.val);
			}
			
			if (signal_data->switchtype == SNGISDN_SWITCH_NI2) {
				if (conEvnt->shift11.eh.pres && conEvnt->ni2OctStr.eh.pres) {
					if (conEvnt->ni2OctStr.str.len == 4 && conEvnt->ni2OctStr.str.val[0] == 0x37) {
						snprintf(ftdmchan->caller_data.aniII, 5, "%.2d", conEvnt->ni2OctStr.str.val[3]);
					}
				}
				
				if (signal_data->facility == SNGISDN_OPT_TRUE && conEvnt->facilityStr.eh.pres) {
					/* Verify whether the Caller Name will come in a subsequent FACILITY message */
					uint16_t ret_val;
					char retrieved_str[255];
					
					ret_val = sng_isdn_retrieve_facility_caller_name(conEvnt->facilityStr.facilityStr.val, conEvnt->facilityStr.facilityStr.len, retrieved_str);
					/*
						return values for "sng_isdn_retrieve_facility_information_following":
						If there will be no information following, or fails to decode IE, returns -1
						If there will be no information following, but current FACILITY IE contains a caller name, returns 0
						If there will be information following, returns 1
					*/

					if (ret_val == 1) {
						ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Expecting Caller name in FACILITY\n");
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_GET_CALLERID);
						/* Launch timer in case we never get a FACILITY msg */
						if (signal_data->facility_timeout) {
							ftdm_sched_timer(signal_data->sched, "facility_timeout", signal_data->facility_timeout, 
									sngisdn_facility_timeout, (void*) sngisdn_info, &sngisdn_info->timers[SNGISDN_TIMER_FACILITY]);
						}
						break;
					} else if (ret_val == 0) {
						strcpy(ftdmchan->caller_data.cid_name, retrieved_str);
					}
				}
			}

			if (signal_data->overlap_dial == SNGISDN_OPT_TRUE && !conEvnt->sndCmplt.eh.pres) {
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_COLLECT);
			} else {
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RING);
			}
			break;
		case FTDM_CHANNEL_STATE_TERMINATING:
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_INFO, "Processing SETUP in TERMINATING state, saving SETUP info for later processing\n");
			ftdm_assert(!sngisdn_test_flag(sngisdn_info, FLAG_GLARE), "Trying to save GLARE info, but we already had a glare\n");
			
			sngisdn_set_flag(sngisdn_info, FLAG_GLARE);

			/* save the SETUP for processing once the channel has gone to DOWN */
			memcpy(&sngisdn_info->glare.setup, conEvnt, sizeof(*conEvnt));
			sngisdn_info->glare.suId = suId;
			sngisdn_info->glare.suInstId = suInstId; /* Do not generate a suInstId now, we will generate when glared call gets extracted */
			sngisdn_info->glare.spInstId = spInstId;
			sngisdn_info->glare.dChan = dChan;
			sngisdn_info->glare.ces = ces;
			
			break;
		case FTDM_CHANNEL_STATE_DIALING:	/* glare */
			if (signal_data->signalling == SNGISDN_SIGNALING_NET) {
				/* Save inbound call info so we can send a RELEASE when this channel goes to a different state */
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Processing SETUP in DIALING state, rejecting inbound call\n");
				sngisdn_set_flag(sngisdn_info, FLAG_DELAYED_REL);

				sngisdn_info->glare.suId = suId;
				sngisdn_info->glare.suInstId = get_unique_suInstId(suId);
				sngisdn_info->glare.spInstId = spInstId;

				sngisdn_info->glare.dChan = dChan;
				sngisdn_info->glare.ces = ces;
				ftdmchan->caller_data.hangup_cause = 0x2C; /* Channel requested not available */
				ftdm_sched_timer(signal_data->sched, "delayed_release", 1, sngisdn_delayed_release, (void*) sngisdn_info, NULL);
			} else {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_INFO, "Processing SETUP in DIALING state, saving SETUP info for later processing\n");
				
				/* the flag the channel as having a collision */
				ftdm_assert(!sngisdn_test_flag(sngisdn_info, FLAG_GLARE), "Trying to save GLARE info, but we already had a glare");
				sngisdn_set_flag(sngisdn_info, FLAG_GLARE);

				/* save the SETUP for processing once the channel has gone to DOWN */
				memcpy(&sngisdn_info->glare.setup, conEvnt, sizeof(*conEvnt));
				sngisdn_info->glare.suId = suId;
				sngisdn_info->glare.suInstId = suInstId; /* Do not generate a suInstId now, we will generate when glared call gets extracted */
				sngisdn_info->glare.spInstId = spInstId;
				sngisdn_info->glare.dChan = dChan;
				sngisdn_info->glare.ces = ces;

				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
			}
			break;
		default:
			ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Processing SETUP in an invalid state (%s)\n", ftdm_channel_state2str(ftdmchan->state));
			break;
	}
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

/* Remote side transmit a CONNECT or CONNECT ACK */
void sngisdn_process_con_cfm (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	int16_t suId = sngisdn_event->suId;
	uint32_t suInstId = sngisdn_event->suInstId;
	uint32_t spInstId = sngisdn_event->spInstId;
	uint8_t ces = sngisdn_event->ces;
	sngisdn_chan_data_t *sngisdn_info = sngisdn_event->sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	
	/* Function does not require any info from conStEvnt struct for now */
	/* CnStEvnt *cnStEvnt = &sngisdn_event->event.cnStEvnt; */	
				
	ftdm_assert(!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE), "State change flag pending\n");
	
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Processing CONNECT/CONNECT ACK (suId:%u suInstId:%u spInstId:%u ces:%d)\n", suId, suInstId, spInstId, ces);

	if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP &&
		((sngisdn_span_data_t*)ftdmchan->span->signal_data)->signalling == SNGISDN_SIGNALING_NET) {

		if(sngisdn_info->ces == CES_MNGMNT) {
			/* We assign the call to the first TE */
			sngisdn_info->ces = ces;
		} else {
			/* We already assigned this call, do nothing */
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Call already assigned, ignoring connect\n");
			ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
			return;
		}
	}

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
		switch(ftdmchan->state) {
			case FTDM_CHANNEL_STATE_PROGRESS:
			case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
			case FTDM_CHANNEL_STATE_DIALING:
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_UP);
				break;
			case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
			case FTDM_CHANNEL_STATE_HANGUP:
				/* Race condition, we just hung up the call - ignore this message */
				break;
			default:
				ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Processing CONNECT/CONNECT ACK in an invalid state (%s)\n", ftdm_channel_state2str(ftdmchan->state));

				/* Start the disconnect procedure */
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
				break;
		}
	} else {
		switch(ftdmchan->state) {
			case FTDM_CHANNEL_STATE_UP:
				/* This is the only valid state we should get a CONNECT ACK on */
				/* do nothing */
				break;
			case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
				/* Race condition, We just hung up an incoming call right after we sent a CONNECT - ignore this message */
				break;
			default:
				ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Processing CONNECT/CONNECT ACK in an invalid state (%s)\n", ftdm_channel_state2str(ftdmchan->state));
				
				/* Start the disconnect procedure */
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
				break;
		}
	}

	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_process_cnst_ind (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	int16_t suId = sngisdn_event->suId;
	uint32_t suInstId = sngisdn_event->suInstId;
	uint32_t spInstId = sngisdn_event->spInstId;
	uint8_t ces = sngisdn_event->ces;
	uint8_t evntType = sngisdn_event->evntType;
	
	sngisdn_chan_data_t *sngisdn_info = sngisdn_event->sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;
	
	CnStEvnt *cnStEvnt = &sngisdn_event->event.cnStEvnt;

	ftdm_assert(!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE), "State change flag pending\n");

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Processing %s (suId:%u suInstId:%u spInstId:%u ces:%d)\n",
													(evntType == MI_ALERTING)?"ALERT":
													(evntType == MI_CALLPROC)?"PROCEED":
													(evntType == MI_PROGRESS)?"PROGRESS":
													(evntType == MI_SETUPACK)?"SETUP ACK":
															(evntType == MI_INFO)?"INFO":"UNKNOWN",
															suId, suInstId, spInstId, ces);
	
	switch(evntType) {
		case MI_PROGRESS:
			if (signal_data->switchtype == SNGISDN_SWITCH_NI2 &&
						 cnStEvnt->causeDgn[0].eh.pres && cnStEvnt->causeDgn[0].causeVal.pres) {

				switch(cnStEvnt->causeDgn[0].causeVal.val) {
					case 17:	/* User Busy */
					case 18:	/* No User responding */
					case 19:	/* User alerting, no answer */
					case 21:	/* Call rejected, the called party does not with to accept this call */
					case 27:	/* Destination out of order */
					case 31:	/* Normal, unspecified */
					case 34:	/* Circuit/Channel congestion */
					case 41:	/* Temporary failure */
					case 42:	/* Switching equipment is experiencing a period of high traffic */
					case 47:	/* Resource unavailable */
					case 58:	/* Bearer Capability not available */
					case 63:	/* Service or option not available */
					case 65:	/* Bearer Cap not implemented, not supported */
					case 79:	/* Service or option not implemented, unspecified */
						ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Cause requires disconnect (cause:%d)\n", cnStEvnt->causeDgn[0].causeVal.val);
						ftdmchan->caller_data.hangup_cause = cnStEvnt->causeDgn[0].causeVal.val;
						
						sngisdn_set_flag(sngisdn_info, FLAG_SEND_DISC);
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
						goto sngisdn_process_cnst_ind_end;
				}
			}
			/* fall-through */
		case MI_ALERTING:
		case MI_CALLPROC:
		
			switch(ftdmchan->state) {
				case FTDM_CHANNEL_STATE_DIALING:					
					if (evntType == MI_PROGRESS ||
						(cnStEvnt->progInd.eh.pres && cnStEvnt->progInd.progDesc.val == IN_PD_IBAVAIL)) {
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);
					} else {
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS);
					}
					break;
				case FTDM_CHANNEL_STATE_PROGRESS:
					if (evntType == MI_PROGRESS ||
						(cnStEvnt->progInd.eh.pres && cnStEvnt->progInd.progDesc.val == IN_PD_IBAVAIL)) {
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);
					}
					break;
				case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
					/* We are already in progress media, we can't go to any higher state except up */
					/* Do nothing */
					break;
				default:
					ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Processing ALERT/PROCEED/PROGRESS in an invalid state (%s)\n", ftdm_channel_state2str(ftdmchan->state));

					/* Start the disconnect procedure */
					ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
					break;
			}
			break;
		case MI_SETUPACK:
			ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "Processing SETUP_ACK, but overlap sending not implemented (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
			break;
		case MI_INFO:
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Processing INFO (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);

			if (cnStEvnt->cdPtyNmb.eh.pres) {
				switch(ftdmchan->state) {
					case FTDM_CHANNEL_STATE_COLLECT:
					{
						ftdm_size_t min_digits = ((sngisdn_span_data_t*)ftdmchan->span->signal_data)->min_digits;
						ftdm_size_t num_digits;

						cpy_called_num_from_stack(&ftdmchan->caller_data, &cnStEvnt->cdPtyNmb);
						num_digits = strlen(ftdmchan->caller_data.dnis.digits);

						if (cnStEvnt->sndCmplt.eh.pres || num_digits >= min_digits) {
							ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RING);
						} else {
							ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "received %d of %d digits\n", num_digits, min_digits);
						}
					}
					break;
					case FTDM_CHANNEL_STATE_RING:
					case FTDM_CHANNEL_STATE_PROGRESS:
					case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
					case FTDM_CHANNEL_STATE_UP:
						ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "Receiving more digits %s, but we already proceeded with call\n", cnStEvnt->cdPtyNmb.nmbDigits.val);
						break;
					default:
						ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "\n", suId, suInstId, spInstId);
						break;
				}
			}

			break;
		default:
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Unhandled STATUS event\n");
			break;
	}

sngisdn_process_cnst_ind_end:
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_process_disc_ind (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	int16_t suId = sngisdn_event->suId;
	uint32_t suInstId = sngisdn_event->suInstId;
	uint32_t spInstId = sngisdn_event->spInstId;
	sngisdn_chan_data_t *sngisdn_info = sngisdn_event->sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	
	DiscEvnt *discEvnt = &sngisdn_event->event.discEvnt;

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Processing DISCONNECT (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);

	ftdm_assert(!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE), "State change flag pending\n");
	switch (ftdmchan->state) {
		case FTDM_CHANNEL_STATE_RING:
		case FTDM_CHANNEL_STATE_DIALING:
		case FTDM_CHANNEL_STATE_PROGRESS:
		case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
		case FTDM_CHANNEL_STATE_UP:
			if (discEvnt->causeDgn[0].eh.pres && discEvnt->causeDgn[0].causeVal.pres) {
				ftdmchan->caller_data.hangup_cause = discEvnt->causeDgn[0].causeVal.val;
			} else {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "DISCONNECT did not have a cause code\n");
				ftdmchan->caller_data.hangup_cause = 0;
			}
			sngisdn_set_flag(sngisdn_info, FLAG_REMOTE_REL);
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
			break;
		case FTDM_CHANNEL_STATE_COLLECT:
		case FTDM_CHANNEL_STATE_GET_CALLERID:
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_CANCEL);
			break;
		case FTDM_CHANNEL_STATE_DOWN:
			/* somehow we are in down, nothing we can do locally */
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Received DISCONNECT but we are in DOWN state\n");
			break;
		case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
			/* This is a race condition. We just sent a DISCONNECT, on this channel */
			/* Do nothing */
			break;
		default:
			ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Received DISCONNECT in an invalid state (%s)\n",
						  ftdm_channel_state2str(ftdmchan->state));
			/* start reset procedure */

			/* Start the release procedure */
			ftdm_set_flag(sngisdn_info, FLAG_REMOTE_REL);
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
			break;
	}

	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_process_rel_ind (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	int16_t suId = sngisdn_event->suId;
	uint32_t suInstId = sngisdn_event->suInstId;
	uint32_t spInstId = sngisdn_event->spInstId;
	sngisdn_chan_data_t *sngisdn_info = sngisdn_event->sngisdn_info;	
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	
	RelEvnt *relEvnt = &sngisdn_event->event.relEvnt;

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Processing RELEASE/RELEASE COMPLETE (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	
	ftdm_assert(!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE), "State change flag pending\n");
	
	if ((suInstId && (sngisdn_info->glare.suInstId == suInstId)) ||
		(spInstId && (sngisdn_info->glare.spInstId == spInstId))) {

		/* This hangup is for a glared saved call */
		ftdm_clear_flag(sngisdn_info, FLAG_DELAYED_REL);
		clear_call_glare_data(sngisdn_info);

		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	/* check whether the ftdm channel is in a state to accept a call */
	switch (ftdmchan->state) {
		case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
			/* go to DOWN */
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
			break;
		case FTDM_CHANNEL_STATE_DOWN:
			/* do nothing, just drop the message */
			break;
		case FTDM_CHANNEL_STATE_DIALING:
			/* Remote side rejected our SETUP message on outbound call */
			if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SIG_UP)) {
				sng_isdn_set_avail_rate(ftdmchan->span, SNGISDN_AVAIL_DOWN);
			}
			/* fall-through */
		case FTDM_CHANNEL_STATE_PROGRESS:
		case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
		case FTDM_CHANNEL_STATE_UP:
		case FTDM_CHANNEL_STATE_RING:
			/* If we previously had a glare on this channel,
			this RELEASE could be for the previous call.  Confirm whether call_data has
			not changed while we were waiting for ftdmchan->mutex by comparing suInstId's */
			if (((sngisdn_chan_data_t*)ftdmchan->call_data)->suInstId == suInstId ||
									((sngisdn_chan_data_t*)ftdmchan->call_data)->spInstId == spInstId) {
				if (relEvnt->causeDgn[0].eh.pres && relEvnt->causeDgn[0].causeVal.pres) {
					ftdmchan->caller_data.hangup_cause = relEvnt->causeDgn[0].causeVal.val;
					ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "cause:%d\n", ftdmchan->caller_data.hangup_cause);
				} else {
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "RELEASE COMPLETE did not have a cause code\n");
					ftdmchan->caller_data.hangup_cause = 0;
				}

				sngisdn_set_flag(sngisdn_info, FLAG_REMOTE_ABORT);
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);

			} else {
				ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "RELEASE was for previous call (suInstId:%u spInstId:%u)\n", suInstId, spInstId);
			}
			break;
		case FTDM_CHANNEL_STATE_COLLECT:
		case FTDM_CHANNEL_STATE_GET_CALLERID:
			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_CANCEL);
			break;
		case FTDM_CHANNEL_STATE_TERMINATING:
			if (sngisdn_test_flag(sngisdn_info, FLAG_GLARE) &&
								sngisdn_info->glare.suInstId != suInstId) {
				/* This release if for the outbound call that we already started clearing */

				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Received RELEASE for local glared call\n");
				sngisdn_set_flag(sngisdn_info, FLAG_REMOTE_ABORT);
			} else {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Received release before we could clear local call\n");
				/* FS core took too long to respond to the SIG STOP event */
				sngisdn_set_flag(sngisdn_info, FLAG_REMOTE_ABORT);
				/* set abort flag so that we do not transmit another release complete on this channel once FS core is done */
			}
			break;
		default:
			ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Received RELEASE in an invalid state (%s)\n",
							ftdm_channel_state2str(ftdmchan->state));

			break;
	}


	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_process_dat_ind (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);	
	int16_t suId = sngisdn_event->suId;
	uint32_t suInstId = sngisdn_event->suInstId;
	uint32_t spInstId = sngisdn_event->spInstId;

	sngisdn_chan_data_t *sngisdn_info = sngisdn_event->sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;

	/* Function does not require any info from infoEvnt struct for now */
	/* InfoEvnt *infoEvnt = &sngisdn_event->event.infoEvnt; */
	
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Processing DATA IND (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_process_sshl_ind (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	int16_t suId = sngisdn_event->suId;
	uint32_t suInstId = sngisdn_event->suInstId;
	uint32_t spInstId = sngisdn_event->spInstId;

	sngisdn_chan_data_t *sngisdn_info = sngisdn_event->sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;

	/* Function does not require any info from ssHlEvnt struct for now */
	/* SsHlEvnt *ssHlEvnt = &sngisdn_event->event.ssHlEvnt; */
	
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Processing SSHL IND (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_process_sshl_cfm (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	int16_t suId = sngisdn_event->suId;
	uint32_t suInstId = sngisdn_event->suInstId;
	uint32_t spInstId = sngisdn_event->spInstId;
	
	sngisdn_chan_data_t *sngisdn_info = sngisdn_event->sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;

	/* Function does not require any info from ssHlEvnt struct for now */
	/* SsHlEvnt *ssHlEvnt = &sngisdn_event->event.ssHlEvnt; */

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Processing SSHL CFM (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_process_rmrt_ind (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	int16_t suId = sngisdn_event->suId;
	uint32_t suInstId = sngisdn_event->suInstId;
	uint32_t spInstId = sngisdn_event->spInstId;

	sngisdn_chan_data_t *sngisdn_info = sngisdn_event->sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;

	/* Function does not require any info from ssHlEvnt struct for now */
	/* RmRtEvnt *rmRtEvnt = &sngisdn_event->event.rmRtEvnt; */

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Processing RESUME/RETRIEVE IND (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_process_rmrt_cfm (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	int16_t suId = sngisdn_event->suId;
	uint32_t suInstId = sngisdn_event->suInstId;
	uint32_t spInstId = sngisdn_event->spInstId;

	sngisdn_chan_data_t *sngisdn_info = sngisdn_event->sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	
	/* Function does not require any info from ssHlEvnt struct for now */
	/* RmRtEvnt *rmRtEvnt = &sngisdn_event->event.rmRtEvnt; */
	
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Processing RESUME/RETRIEVE CFM (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_process_flc_ind (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	int16_t suId = sngisdn_event->suId;
	uint32_t suInstId = sngisdn_event->suInstId;
	uint32_t spInstId = sngisdn_event->spInstId;

	sngisdn_chan_data_t *sngisdn_info = sngisdn_event->sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;

	/* Function does not require any info from ssHlEvnt struct for now */
	/* StaEvnt *staEvnt = &sngisdn_event->event.staEvnt; */

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Processing FLOW CONTROL IND (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_process_fac_ind (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	int16_t suId = sngisdn_event->suId;
	uint32_t suInstId = sngisdn_event->suInstId;
	uint32_t spInstId = sngisdn_event->spInstId;

	sngisdn_chan_data_t *sngisdn_info = sngisdn_event->sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	FacEvnt *facEvnt = &sngisdn_event->event.facEvnt;

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Processing FACILITY IND (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);

	switch (ftdmchan->state) {
		case FTDM_CHANNEL_STATE_GET_CALLERID:
			/* Update the caller ID Name */
			if (facEvnt->facElmt.facStr.pres) {
				char retrieved_str[255];
				
				/* return values for "sng_isdn_retrieve_facility_information_following":
				If there will be no information following, or fails to decode IE, returns -1
				If there will be no information following, but current FACILITY IE contains a caller name, returns 0
				If there will be information following, returns 1
				*/
				
				if (sng_isdn_retrieve_facility_caller_name(&facEvnt->facElmt.facStr.val[2], facEvnt->facElmt.facStr.len, retrieved_str) == 0) {
					strcpy(ftdmchan->caller_data.cid_name, retrieved_str);
				} else {
					ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Failed to retrieve Caller Name from Facility IE\n");
				}
				if (signal_data->facility_timeout) {
					/* Cancel facility timeout */
					ftdm_sched_cancel_timer(signal_data->sched, sngisdn_info->timers[SNGISDN_TIMER_FACILITY]);
				}
			}

			ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RING);
			break;
		case FTDM_CHANNEL_STATE_RING:
			/* We received the caller ID Name in FACILITY, but its too late, facility-timeout already occurred */
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "FACILITY received, but we already proceeded with call\n");
			break;
		default:
			/* We do not support other FACILITY types for now, so do nothing */
			break;
	}
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_process_sta_cfm (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	
	int16_t suId = sngisdn_event->suId;
	uint32_t suInstId = sngisdn_event->suInstId;
	uint32_t spInstId = sngisdn_event->spInstId;
	sngisdn_chan_data_t *sngisdn_info = sngisdn_event->sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	
	StaEvnt *staEvnt = &sngisdn_event->event.staEvnt;
	
	uint8_t call_state = 0;

	if (staEvnt->callSte.eh.pres && staEvnt->callSte.callGlblSte.pres) {
		call_state = staEvnt->callSte.callGlblSte.val;
	}

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Processing STATUS CONFIRM (suId:%u suInstId:%u spInstId:%u)\n", suId, suInstId, spInstId);

	ftdm_assert(!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE), "State change flag pending\n");

	if (staEvnt->causeDgn[0].eh.pres && staEvnt->causeDgn[0].causeVal.pres) {
		if (staEvnt->callSte.eh.pres && staEvnt->callSte.callGlblSte.pres) {
			call_state = staEvnt->callSte.callGlblSte.val;
		} else {
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Received STATUS without call state\n");
			ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
			return;
		}
		switch (staEvnt->causeDgn[0].causeVal.val) {
			case FTDM_CAUSE_RESPONSE_TO_STATUS_ENQUIRY:
				ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Status Check OK:%d", call_state);
				ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
				return;
			case FTDM_CAUSE_WRONG_CALL_STATE:
				ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "Message incompatible with call state (call_state:%d channel-state:%s cause:%d) (suId:%u suInstId:%u spInstId:%u)\n", call_state, ftdm_channel_state2str(ftdmchan->state), staEvnt->causeDgn[0].causeVal.val, suId, suInstId, spInstId);
				break;
			case FTDM_CAUSE_RECOVERY_ON_TIMER_EXPIRE:
				ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "Recovery on timer expire (call_state:%d channel-state:%s cause:%d) (suId:%u suInstId:%u spInstId:%u)\n", call_state, ftdm_channel_state2str(ftdmchan->state), staEvnt->causeDgn[0].causeVal.val, suId, suInstId, spInstId);
				break;
			default:
				ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "STATUS CONFIRM (call_state:%d channel-state:%s cause:%d) (suId:%u suInstId:%u spInstId:%u)\n", call_state, ftdm_channel_state2str(ftdmchan->state), staEvnt->causeDgn[0].causeVal.val, suId, suInstId, spInstId);
				break;
		}

		/* Section 4.3.30 from INT Interface - Service Definition */
		ftdmchan->caller_data.hangup_cause = staEvnt->causeDgn[0].causeVal.val;
		
		switch(call_state) {
			/* Sere ITU-T Q931 for definition of call states */
			case 0:	/* Remote switch thinks there are no calls on this channel */
				switch (ftdmchan->state) {
					case FTDM_CHANNEL_STATE_COLLECT:
					case FTDM_CHANNEL_STATE_DIALING:
					case FTDM_CHANNEL_STATE_UP:
						sngisdn_set_flag(sngisdn_info, FLAG_REMOTE_ABORT);
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
						break;
					case FTDM_CHANNEL_STATE_TERMINATING:
						/* We are in the process of clearing local states,
						just make sure we will not send any messages to remote switch */
						sngisdn_set_flag(sngisdn_info, FLAG_REMOTE_ABORT);
						break;
					case FTDM_CHANNEL_STATE_HANGUP:
						/* This cannot happen, state_advance always sets
						ftdmchan to STATE_HANGUP_COMPLETE when in STATE_HANGUP
						and we called check_for_state_change earlier so something is very wrong here!!! */
						ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "How can we we in FTDM_CHANNEL_STATE_HANGUP after checking for state change?\n");
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
						break;
					case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
						/* We were waiting for remote switch to send RELEASE COMPLETE
						but this will not happen, so just clear local state */
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
						break;
					case FTDM_CHANNEL_STATE_DOWN:
						/* If our local state is down as well, then there is nothing to do */
						break;
					default:
						ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Don't know how to handle incompatible state. remote call state:%d our state:%s\n", call_state, ftdm_channel_state2str(ftdmchan->state));
						break;
				}
				break;
			case 1:
				switch (ftdmchan->state) {
					case FTDM_CHANNEL_STATE_UP:
						/* Remote side is still waiting for our CONNECT message */
						if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
							ftdm_sched_timer(((sngisdn_span_data_t*)ftdmchan->span->signal_data)->sched, "delayed_connect", 1, sngisdn_delayed_connect, (void*) sngisdn_info, NULL);
							break;
						}
						/* Fall-through */
					default:
						ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Don't know how to handle incompatible state. remote call state:%d our state:%s\n", call_state, ftdm_channel_state2str(ftdmchan->state));
						break;
				}
				break;
				case 2: /* overlap sending/receiving */
					switch (ftdmchan->state) {
						case FTDM_CHANNEL_STATE_COLLECT:
							/* T302 Timeout reached */
							/* Send the call to user, and see if they accept it */
							ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "T302 Timer expired, proceeding with call\n");
							ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RING);
							break;
						case FTDM_CHANNEL_STATE_PROGRESS:
						case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
							ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Remote switch expecting OVERLAP receive, but we are already PROCEEDING\n");
							sngisdn_snd_disconnect(ftdmchan);
							break;
						case FTDM_CHANNEL_STATE_DOWN:
						case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
							/* We hung up locally, but remote switch doesn't know send disconnect again*/
							sngisdn_snd_disconnect(ftdmchan);
							break;
						default:
							ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Don't know how to handle incompatible state. remote call state:%d our state:%s\n", call_state, ftdm_channel_state2str(ftdmchan->state));
							break;
					}
					break;
			case 3:
				switch (ftdmchan->state) {
					case FTDM_CHANNEL_STATE_PROGRESS:
						/* T310 timer has expired */
						ftdmchan->caller_data.hangup_cause = staEvnt->causeDgn[0].causeVal.val;
						ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "T310 Timer expired, hanging up call\n");
						sngisdn_set_flag(sngisdn_info, FLAG_SEND_DISC);
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);

						break;
					case FTDM_CHANNEL_STATE_UP:
						/* Remote side is still waiting for our CONNECT message */
						if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OUTBOUND)) {
							ftdm_sched_timer(((sngisdn_span_data_t*)ftdmchan->span->signal_data)->sched, "delayed_connect", 1, sngisdn_delayed_connect, (void*) sngisdn_info, NULL);
							break;
						}
						/* Fall-through */
					default:
						ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Don't know how to handle incompatible state. remote call state:%d our state:%s\n", call_state, ftdm_channel_state2str(ftdmchan->state));
						break;
				}
				break;
			case 8: /* Remote switch is in "Connect Request state" */
				switch (ftdmchan->state) {
					case FTDM_CHANNEL_STATE_UP:
						/* This is ok. We sent a Connect, and we are waiting for a connect ack */
						/* Do nothing */
						break;
					case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
						/* We hung up locally, but remote switch doesn't know send disconnect again*/
						sngisdn_snd_disconnect(ftdmchan);
						break;
					default:
						ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Don't know how to handle incompatible state. remote call state:%d our state:%s\n", call_state, ftdm_channel_state2str(ftdmchan->state));
						break;
				}
				break;
			case 9: /* Remote switch is in "Incoming call proceeding" state */
				switch (ftdmchan->state) {
					case FTDM_CHANNEL_STATE_PROGRESS:
					case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
					case FTDM_CHANNEL_STATE_GET_CALLERID:
						/* Do nothing */
						break;
					case FTDM_CHANNEL_STATE_UP:
						/* Remote switch missed our CONNECT message, re-send */
						ftdm_sched_timer(((sngisdn_span_data_t*)ftdmchan->span->signal_data)->sched, "delayed_connect", 1, sngisdn_delayed_connect, (void*) sngisdn_info, NULL);
						break;
					default:
						ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Don't know how to handle incompatible state. remote call state:%d our state:%s\n", call_state, ftdm_channel_state2str(ftdmchan->state));
						break;
				}
				break;
			case 10: /* Remote switch is in active state */
				switch (ftdmchan->state) {
					case FTDM_CHANNEL_STATE_UP:
						/* This is ok, they are in active state and we are in active state */
						/* Do nothing */
						break;
					case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
						/* We sent a disconnect message, but remote side missed it ? */
						ftdm_sched_timer(((sngisdn_span_data_t*)ftdmchan->span->signal_data)->sched, "delayed_disconnect", 1, sngisdn_delayed_disconnect, (void*) sngisdn_info, NULL);
						break;
					default:
						ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Don't know how to handle incompatible state. remote call state:%d our state:%s\n", call_state, ftdm_channel_state2str(ftdmchan->state));
						break;
				}
				break;
			case 12: /* We received a disconnect indication */
				switch (ftdmchan->state) {
					case FTDM_CHANNEL_STATE_TERMINATING:
						/* We are already waiting for user app to handle the disconnect, do nothing */
						break;
					default:
						ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Don't know how to handle incompatible state. remote call state:%d our state:%s\n", call_state, ftdm_channel_state2str(ftdmchan->state));
						break;
				}
				break;
			case 22:
				switch (ftdmchan->state) {
					case FTDM_CHANNEL_STATE_UP:
						/* Stack is in the process of clearing the call*/
						ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
						break;
					case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
						/* Do nothing as we will get a RELEASE COMPLETE */
						break;
					default:
						ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Don't know how to handle incompatible state. remote call state:%d our state:%s\n", call_state, ftdm_channel_state2str(ftdmchan->state));
						//ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
						break;
				}
				break;
			case 25: /* Overlap receiving */
				switch (ftdmchan->state) {
					case FTDM_CHANNEL_STATE_COLLECT:
						/* do nothing */
						break;
					default:
						ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Don't know how to handle incompatible state. remote call state:%d our state:%s\n", call_state, ftdm_channel_state2str(ftdmchan->state));
						//ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
						break;
				}
				break;
			default:
				ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Don't know how to handle incompatible state. remote call state:%d our state:%s\n", call_state, ftdm_channel_state2str(ftdmchan->state));
						//ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
				break;
		}
	}

	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}


void sngisdn_process_srv_ind (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	int16_t suId = sngisdn_event->suId;
	int16_t dChan = sngisdn_event->dChan;
	uint8_t ces = sngisdn_event->ces;

	/* Function does not require any info from ssHlEvnt struct for now */
	/*Srv *srvEvnt = &sngisdn_event->event.srvEvnt;*/
	
	ftdm_log(FTDM_LOG_DEBUG, "Processing SERVICE IND (suId:%u dChan:%d ces:%d)\n", suId, dChan, ces);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_process_srv_cfm (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	int16_t suId = sngisdn_event->suId;
	int16_t dChan = sngisdn_event->dChan;
	uint8_t ces = sngisdn_event->ces;

	/* Function does not require any info from ssHlEvnt struct for now */
	/*Srv *srvEvnt = &sngisdn_event->event.srvEvnt;*/
	
	ftdm_log(FTDM_LOG_DEBUG, "Processing SERVICE CFM (suId:%u dChan:%d ces:%d)\n", suId, dChan, ces);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_process_rst_cfm (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	int16_t suId = sngisdn_event->suId;
	int16_t dChan = sngisdn_event->dChan;
	uint8_t ces = sngisdn_event->ces;
	uint8_t evntType = sngisdn_event->evntType;

	/* Function does not require any info from ssHlEvnt struct for now */
	/*Rst *rstEvnt = &sngisdn_event->event.rstEvnt;*/
	
	ftdm_log(FTDM_LOG_DEBUG, "Processing RESTART CFM (suId:%u dChan:%d ces:%d type:%d)\n", suId, dChan, ces, evntType);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}


void sngisdn_process_rst_ind (sngisdn_event_data_t *sngisdn_event)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	int16_t suId = sngisdn_event->suId;
	int16_t dChan = sngisdn_event->dChan;
	uint8_t ces = sngisdn_event->ces;
	uint8_t evntType = sngisdn_event->evntType;

	/* Function does not require any info from ssHlEvnt struct for now */
	/*Rst *rstEvnt = &sngisdn_event->event.rstEvnt;*/
		
	ftdm_log(FTDM_LOG_DEBUG, "Processing RESTART CFM (suId:%u dChan:%d ces:%d %s)\n", suId, dChan, ces,
													(evntType == IN_LNK_DWN)?"LNK_DOWN":
													(evntType == IN_LNK_UP)?"LNK_UP":
													(evntType == IN_INDCHAN)?"b-channel":
													(evntType == IN_LNK_DWN_DM_RLS)?"Nfas service procedures":
													(evntType == IN_SWCHD_BU_DCHAN)?"NFAS switchover to backup":"Unknown");
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}
