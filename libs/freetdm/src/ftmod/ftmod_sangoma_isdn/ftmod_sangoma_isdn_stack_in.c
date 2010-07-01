/*
 * Copyright (c) 2010, Sangoma Technologies
 * David Yat Sin <davidy@sangoma.com>
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

extern ftdm_status_t cpy_calling_num_from_sngisdn(ftdm_caller_data_t *ftdm, CgPtyNmb *cgPtyNmb);
extern ftdm_status_t cpy_called_num_from_sngisdn(ftdm_caller_data_t *ftdm, CdPtyNmb *cdPtyNmb);
extern ftdm_status_t cpy_called_name_from_sngisdn(ftdm_caller_data_t *ftdm, CgPtyNmb *cgPtyNmb);
extern ftdm_status_t cpy_calling_name_from_sngisdn(ftdm_caller_data_t *ftdm, ConEvnt *conEvnt);
extern void sngisdn_trace_q921(char* str, uint8_t* data, uint32_t data_len);
extern void sngisdn_trace_q931(char* str, uint8_t* data, uint32_t data_len);
extern void get_memory_info(void);

extern ftdm_sngisdn_data_t	g_sngisdn_data;

#define MAX_DECODE_STR_LEN 2000

/* Remote side transmit a SETUP */
void sngisdn_rcv_con_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, ConEvnt *conEvnt, int16_t dChan, uint8_t ces)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	uint8_t bchan_no = 0;
	sngisdn_chan_data_t *sngisdn_info;
	ftdm_channel_t *ftdmchan;
	/*sngisdn_span_data_t *span_info;*/
	
	ftdm_log(FTDM_LOG_DEBUG, "%s suId:%d suInstId:%d spInstId:%d dChan:%d ces:%d\n", __FUNCTION__, suId, suInstId, spInstId, dChan, ces);

	ftdm_assert(g_sngisdn_data.ccs[suId].activation_done != 0, "Con Ind on unconfigured cc\n");
	ftdm_assert(g_sngisdn_data.dchans[dChan].num_spans != 0, "Con Ind on unconfigured dchan\n");
	ftdm_assert(g_sngisdn_data.ccs[suId].active_spInstIds[spInstId] == NULL, "Con Ind on busy spInstId");
	
	if (conEvnt->chanId.eh.pres != PRSNT_NODEF) {
		/* TODO: Implement me */
		ftdm_log(FTDM_LOG_ERROR, "Incoming call without Channel Id not supported yet\n");
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	if (conEvnt->chanId.chanNmbSlotMap.pres) {
		bchan_no = conEvnt->chanId.chanNmbSlotMap.val[0];
	} else if (conEvnt->chanId.infoChanSel.pres) {
		bchan_no = conEvnt->chanId.infoChanSel.val;
	}

	if (!bchan_no) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to obtain b-channel number from SETUP message\n");
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	if (g_sngisdn_data.dchans[dChan].channels[bchan_no] == NULL) {
		ftdm_log(FTDM_LOG_ERROR, "Incoming call on unconfigured b-channel:%d\n", bchan_no);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	sngisdn_info = g_sngisdn_data.dchans[dChan].channels[bchan_no];
	ftdmchan = sngisdn_info->ftdmchan;
	ftdm_mutex_lock(ftdmchan->mutex);

	/* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan) != FTDM_SUCCESS) {
        ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Failed to wait for pending state change\n");
        ftdm_mutex_unlock(ftdmchan->mutex);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    }

	ftdm_log_chan_msg(ftdmchan, FTDM_LOG_INFO, "Received SETUP\n");
	switch (ftdmchan->state){
		case FTDM_CHANNEL_STATE_DOWN: /* Proper state to receive a SETUP */
			sngisdn_info->suInstId = get_unique_suInstId(suId);
			sngisdn_info->spInstId = spInstId;
			g_sngisdn_data.ccs[suId].active_spInstIds[spInstId] = sngisdn_info;
			g_sngisdn_data.ccs[suId].active_suInstIds[sngisdn_info->suInstId] = sngisdn_info;

			/* try to open the channel */
			if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Failed to open channel");
				sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_REL);
				ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_NORMAL_TEMPORARY_FAILURE;
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_CANCEL);
			} else {
				/* Fill in call information */
				cpy_calling_num_from_sngisdn(&ftdmchan->caller_data, &conEvnt->cgPtyNmb);
				cpy_called_num_from_sngisdn(&ftdmchan->caller_data, &conEvnt->cdPtyNmb);
				cpy_calling_name_from_sngisdn(&ftdmchan->caller_data, conEvnt);

				/* Get ani2 */
#if 0
				/* TODO: confirm that this works in the field */
				if (conEvnt->niOperSysAcc.eh.pres) {
					if (conEvnt->niOperSysAcc.typeAcc.pres) {
						ftdmchan->caller_data.aniII = (uint8_t)conEvnt->niOperSysAcc.typeAcc.val;
						ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Received ANI: type of access:%x", conEvnt->niOperSysAcc.typeAcc.val);
					}
					if (conEvnt->niOperSysAcc.typeServ.pres) {
						ftdmchan->caller_data.aniII = (uint8_t)conEvnt->niOperSysAcc.typeServ.val;
						ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Received ANI: type of service:%x", conEvnt->niOperSysAcc.typeServ.val);
					}
				}
#endif

				/* set the state of the channel to collecting...the rest is done by the chan monitor */
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_COLLECT);

			}
			break;
		case FTDM_CHANNEL_STATE_DIALING:	/* glare */
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Received SETUP in DIALING state, glare, queueing incoming call\n");
			 /* the flag the channel as having a collision */
			sngisdn_set_flag(sngisdn_info, FLAG_GLARE);

			/* save the SETUP for processing once the channel has gone to DOWN */
			memcpy(&sngisdn_info->glare.setup, conEvnt, sizeof(*conEvnt));
			sngisdn_info->glare.suId = suId;
			sngisdn_info->glare.suInstId = suInstId;
			sngisdn_info->glare.spInstId = spInstId;
			sngisdn_info->glare.dChan = dChan;
			sngisdn_info->glare.ces = ces;
			break;
		default:
			ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Received SETUP in an invalid state (%s)\n", ftdm_channel_state2str(ftdmchan->state));
			break;
	}
	ftdm_mutex_unlock(ftdmchan->mutex);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

/* Remote side transmit a CONNECT or CONNECT ACK */
void sngisdn_rcv_con_cfm (int16_t suId, uint32_t suInstId, uint32_t spInstId, CnStEvnt *cnStEvnt, int16_t dChan, uint8_t ces)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	sngisdn_chan_data_t *sngisdn_info;
	ftdm_channel_t *ftdmchan;

	ftdm_log(FTDM_LOG_DEBUG, "%s suId:%d suInstId:%d spInstId:%d dChan:%d ces:%d\n", __FUNCTION__, suId, suInstId, spInstId, dChan, ces);

	
	ftdm_assert(g_sngisdn_data.ccs[suId].activation_done != 0, "Con Ind on unconfigured cc\n");
	ftdm_assert(g_sngisdn_data.dchans[dChan].num_spans != 0, "Con Ind on unconfigured dchan\n");

	if (get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%d suInstId:%d spInstId:%d\n", suId, suInstId, spInstId);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	ftdmchan = (ftdm_channel_t*)sngisdn_info->ftdmchan;

	if (!sngisdn_info->spInstId) {
		sngisdn_info->spInstId = spInstId;
		g_sngisdn_data.ccs[suId].active_spInstIds[spInstId] = sngisdn_info;
	}

	ftdm_log_chan_msg(ftdmchan, FTDM_LOG_INFO, "Received CONNECT/CONNECT ACK\n");
	ftdm_mutex_lock(ftdmchan->mutex);

	/* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan) != FTDM_SUCCESS) {
        ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Failed to wait for pending state change\n");
        ftdm_mutex_unlock(ftdmchan->mutex);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    }

	if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP &&
		((sngisdn_span_data_t*)ftdmchan->span->signal_data)->signalling == SNGISDN_SIGNALING_NET) {

		if(sngisdn_info->ces == CES_MNGMNT) {
			/* We assign the call to the first TE */
			sngisdn_info->ces = ces;
		} else {
			/* We already assigned this call, do nothing */
			ftdm_mutex_unlock(ftdmchan->mutex);
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
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);
				break;
			default:
				ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Received CONNECT/CONNECT ACK in an invalid state (%s)\n",
						ftdm_channel_state2str(ftdmchan->state));

				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
				break;
		}
	} else {
		switch(ftdmchan->state) {
			case FTDM_CHANNEL_STATE_UP:
				/* This is the only valid state we should get a CONNECT ACK on */
				/* do nothing */
				break;
			default:
				ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Received CONNECT/CONNECT ACK in an invalid state (%s)\n", ftdm_channel_state2str(ftdmchan->state));
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
				break;
		}
	}
	
	ftdm_mutex_unlock(ftdmchan->mutex);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_cnst_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, CnStEvnt *cnStEvnt, uint8_t evntType, int16_t dChan, uint8_t ces)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	sngisdn_chan_data_t *sngisdn_info;
	ftdm_channel_t *ftdmchan;

	ftdm_log(FTDM_LOG_DEBUG, "%s suId:%d suInstId:%d spInstId:%d dChan:%d ces:%d\n", __FUNCTION__, suId, suInstId, spInstId, dChan, ces);

	ftdm_assert(g_sngisdn_data.ccs[suId].activation_done != 0, "Con Ind on unconfigured cc\n");
	ftdm_assert(g_sngisdn_data.dchans[dChan].num_spans != 0, "Con Ind on unconfigured dchan\n");

	if (get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%d suInstId:%d spInstId:%d\n", suId, suInstId, spInstId);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	if (!sngisdn_info->spInstId) {
		sngisdn_info->spInstId = spInstId;
		g_sngisdn_data.ccs[suId].active_spInstIds[spInstId] = sngisdn_info;
	}

	ftdmchan = (ftdm_channel_t*)sngisdn_info->ftdmchan;
	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Received %s\n",
								(evntType == MI_ALERTING)?"ALERT":
								(evntType == MI_CALLPROC)?"PROCEED":
								(evntType == MI_PROGRESS)?"PROGRESS":"UNKNOWN");
	ftdm_mutex_lock(ftdmchan->mutex);

		/* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan) != FTDM_SUCCESS) {
        ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Failed to wait for pending state change\n");
        ftdm_mutex_unlock(ftdmchan->mutex);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    }

	switch(ftdmchan->state) {
		case FTDM_CHANNEL_STATE_DIALING:
			if (evntType == MI_PROGRESS) {
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);
			} else {
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS);
			}
			break;
		case FTDM_CHANNEL_STATE_PROGRESS:
			if (evntType == MI_PROGRESS) {
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS_MEDIA);
			}
			break;
		case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
			/* Do nothing */
			break;
		default:
			ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Received ALERT/PROCEED/PROGRESS in an invalid state (%s)\n",
					ftdm_channel_state2str(ftdmchan->state));

			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
			break;
	}

	ftdm_mutex_unlock(ftdmchan->mutex);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_disc_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, DiscEvnt *discEvnt)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	sngisdn_chan_data_t *sngisdn_info;
	ftdm_channel_t *ftdmchan = NULL;

	ftdm_log(FTDM_LOG_DEBUG, "%s suId:%d suInstId:%d spInstId:%d\n", __FUNCTION__, suId, suInstId, spInstId);

	ftdm_assert(spInstId != 0, "Received DISCONNECT with invalid id");

	if (spInstId && get_ftdmchan_by_spInstId(suId, spInstId, &sngisdn_info) == FTDM_SUCCESS) {
		ftdmchan = (ftdm_channel_t*)sngisdn_info->ftdmchan;
	} else if (suInstId && get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) == FTDM_SUCCESS) {
		ftdmchan = (ftdm_channel_t*)sngisdn_info->ftdmchan;
	} else {
		ftdm_log(FTDM_LOG_CRIT, "Could not find matching call suId:%d suInstId:%d spInstId:%d\n", suId, suInstId, spInstId);
		return;
	}

	if (!sngisdn_info->spInstId) {
		sngisdn_info->spInstId = spInstId;
		g_sngisdn_data.ccs[suId].active_spInstIds[spInstId] = sngisdn_info;
	}

	ftdm_log_chan_msg(ftdmchan, FTDM_LOG_INFO, "Received DISCONNECT\n");
	ftdm_mutex_lock(ftdmchan->mutex);
	/* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan) != FTDM_SUCCESS) {
        ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Failed to wait for pending state change\n");
        ftdm_mutex_unlock(ftdmchan->mutex);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    }

	switch (ftdmchan->state) {
		case FTDM_CHANNEL_STATE_RING:
		case FTDM_CHANNEL_STATE_DIALING:
		case FTDM_CHANNEL_STATE_PROGRESS:
		case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
		case FTDM_CHANNEL_STATE_COLLECT:
		case FTDM_CHANNEL_STATE_UP:		
			if (discEvnt->causeDgn[0].eh.pres && discEvnt->causeDgn[0].causeVal.pres) {
				ftdmchan->caller_data.hangup_cause = discEvnt->causeDgn[0].causeVal.val;
			} else {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "DISCONNECT did not have a cause code\n");
				ftdmchan->caller_data.hangup_cause = 0;
			}
			sngisdn_set_flag(sngisdn_info, FLAG_REMOTE_REL);
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
			break;
		default:
			ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Received DISCONNECT in an invalid state (%s)\n",
					ftdm_channel_state2str(ftdmchan->state));
			/* start reset procedure */
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
			break;
	}

	ftdm_mutex_unlock(ftdmchan->mutex);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_rel_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, RelEvnt *relEvnt)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_DEBUG, "%s suId:%d suInstId:%d spInstId:%d\n", __FUNCTION__, suId, suInstId, spInstId);

	sngisdn_chan_data_t  *sngisdn_info ;
    ftdm_channel_t      *ftdmchan = NULL;

    /* get the ftdmchan and ss7_chan_data from the circuit */
	if (suInstId && (get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) == FTDM_SUCCESS)) {
		ftdmchan = sngisdn_info->ftdmchan;
	} else if (spInstId && (get_ftdmchan_by_spInstId(suId, spInstId, &sngisdn_info) == FTDM_SUCCESS)) {
		ftdmchan = sngisdn_info->ftdmchan;
	}

	if (ftdmchan == NULL) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to find matching channel suId:%d suInstId:%d spInstId:%d\n", suId, suInstId, spInstId);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	ftdm_log_chan_msg(ftdmchan, FTDM_LOG_INFO, "Received RELEASE/RELEASE COMPLETE\n");
    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Failed to wait for pending state change\n");
        ftdm_mutex_unlock(ftdmchan->mutex);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    };

    /* check whether the ftdm channel is in a state to accept a call */
    switch (ftdmchan->state) {
    case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
        /* go to DOWN */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
        break;
    case FTDM_CHANNEL_STATE_DOWN:
        /* do nothing, just drop the message */
        break;
	case FTDM_CHANNEL_STATE_PROGRESS:
	case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
		/* Remote side sent a SETUP, then a RELEASE COMPLETE to abort call - this is an abort */
		sngisdn_set_flag(sngisdn_info, FLAG_REMOTE_ABORT);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
		break;
	case FTDM_CHANNEL_STATE_DIALING:
		/* Remote side rejected our SETUP message on outbound call */
		sngisdn_set_flag(sngisdn_info, FLAG_REMOTE_ABORT);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
		break;
	case FTDM_CHANNEL_STATE_UP:
		sngisdn_set_flag(sngisdn_info, FLAG_REMOTE_ABORT);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
		break;
    default:
        /* Should just stop the call...but a reset is easier for now (since it does hangup the call) */
		ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Received RELEASE in an invalid state (%s)\n",
					ftdm_channel_state2str(ftdmchan->state));

        /* go to RESTART */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);

        break;
    /**************************************************************************/
    }

    /* unlock the channel */
    ftdm_mutex_unlock(ftdmchan->mutex);

	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_dat_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, InfoEvnt *infoEvnt)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_INFO, "Received DATA IND suId:%d suInstId:%d spInstId:%d\n", suId, suInstId, spInstId);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_sshl_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, SsHlEvnt *ssHlEvnt, uint8_t action)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_INFO, "Received SSHL IND suId:%d suInstId:%d spInstId:%d\n", suId, suInstId, spInstId);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_sshl_cfm (int16_t suId, uint32_t suInstId, uint32_t spInstId, SsHlEvnt *ssHlEvnt, uint8_t action)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_INFO, "Received SSHL CFM suId:%d suInstId:%d spInstId:%d\n", suId, suInstId, spInstId);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_rmrt_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, RmRtEvnt *rmRtEvnt, uint8_t action)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_INFO, "Received RESUME/RETRIEVE ind suId:%d suInstId:%d spInstId:%d\n", suId, suInstId, spInstId);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_rmrt_cfm (int16_t suId, uint32_t suInstId, uint32_t spInstId, RmRtEvnt *rmRtEvnt, uint8_t action)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_INFO, "Received RESUME/RETRIEVE CFM suId:%d suInstId:%d spInstId:%d\n", suId, suInstId, spInstId);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_flc_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, StaEvnt *staEvnt)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_INFO, "Received FLOW CONTROL IND suId:%d suInstId:%d spInstId:%d\n", suId, suInstId, spInstId);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_fac_ind (int16_t suId, uint32_t suInstId, uint32_t spInstId, FacEvnt *facEvnt, uint8_t evntType, int16_t dChan, uint8_t ces)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_INFO, "Received FACILITY IND suId:%d suInstId:%d spInstId:%d\n", suId, suInstId, spInstId);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_sta_cfm ( int16_t suId, uint32_t suInstId, uint32_t spInstId, StaEvnt *staEvnt)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_DEBUG, "%s suId:%d suInstId:%d spInstId:%d\n", __FUNCTION__, suId, suInstId, spInstId);

	sngisdn_chan_data_t  *sngisdn_info ;
    ftdm_channel_t      *ftdmchan = NULL;
	uint8_t 			call_state = 0;

	if (staEvnt->callSte.eh.pres && staEvnt->callSte.callGlblSte.pres) {
		call_state = staEvnt->callSte.callGlblSte.val;
	}

    /* get the ftdmchan and ss7_chan_data from the circuit */
	if (suInstId && (get_ftdmchan_by_suInstId(suId, suInstId, &sngisdn_info) == FTDM_SUCCESS)) {
		ftdmchan = sngisdn_info->ftdmchan;
	} else if (spInstId && (get_ftdmchan_by_spInstId(suId, spInstId, &sngisdn_info) == FTDM_SUCCESS)) {
		ftdmchan = sngisdn_info->ftdmchan;
	}

	if (ftdmchan == NULL) {
		ftdm_log(FTDM_LOG_CRIT, "Failed to find matching channel suId:%d suInstId:%d spInstId:%d\n", suId, suInstId, spInstId);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
		return;
	}

	ftdm_log_chan_msg(ftdmchan, FTDM_LOG_INFO, "Received STATUS CONFIRM\n");
    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "Failed to wait for pending state change\n");
        ftdm_mutex_unlock(ftdmchan->mutex);
		ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    };

	if (staEvnt->causeDgn[0].eh.pres && staEvnt->causeDgn[0].causeVal.pres) {
		if (staEvnt->causeDgn[0].causeVal.val != 30) {
			
			if (staEvnt->callSte.eh.pres && staEvnt->callSte.callGlblSte.pres) {
				call_state = staEvnt->callSte.callGlblSte.val;
				/* Section 4.3.30 from INT Interface - Service Definition */
				ftdmchan->caller_data.hangup_cause = staEvnt->causeDgn[0].causeVal.val;


				/* There is incompatibility between local and remote side call states some Q931 msgs probably got lost - initiate disconnect  */
				ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "Incompatible call states detected, remote side indicated state:%d our state:%s cause:%d\n", call_state, ftdm_channel_state2str(ftdmchan->state), staEvnt->causeDgn[0].causeVal.val);

				switch(call_state) {
					/* Sere ITU-T Q931 for definition of call states */
					case 0:	/* Remote switch thinks there are no calls on this channel */
						switch (ftdmchan->state) {
							case FTDM_CHANNEL_STATE_COLLECT:
							case FTDM_CHANNEL_STATE_DIALING:					
								sngisdn_set_flag(sngisdn_info, FLAG_REMOTE_ABORT);
								ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
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
								ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
								break;
							case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
								/* We were waiting for remote switch to send RELEASE COMPLETE
									but this will not happen, so just clear local state */
								ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
								break;
							case FTDM_CHANNEL_STATE_DOWN:
								/* If our local state is down as well, then there is nothing to do */
								break;
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
					default:
						ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Don't know how to handle incompatible state. remote call state:%d our state:%s\n", call_state, ftdm_channel_state2str(ftdmchan->state));
						ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
						break;					
				}
			} else {
				ftdmchan->caller_data.hangup_cause = staEvnt->causeDgn[0].causeVal.val;
				/* We could not extract the call state */
 				ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Incompatible call states detected, but could not determine call (cause:%d)\n", ftdmchan->caller_data.hangup_cause);
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
			}

		}
    /**************************************************************************/
    }

    /* unlock the channel */
    ftdm_mutex_unlock(ftdmchan->mutex);

	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_srv_ind ( int16_t suId, Srv *srvEvnt, int16_t dChan, uint8_t ces)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_INFO, "Received SERVICE IND suId:%d dChan:%d ces:%d\n", suId, dChan, ces);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_srv_cfm ( int16_t suId, Srv *srvEvnt, int16_t dChan, uint8_t ces)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_INFO, "Received SERVICE CFM suId:%d dChan:%d ces:%d\n", suId, dChan, ces);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_rst_ind (int16_t suId, Rst *rstEvnt, int16_t dChan, uint8_t ces, uint8_t evtType)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_INFO, "Received RESTART IND suId:%d dChan:%d ces:%d\n", suId, dChan, ces);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}


void sngisdn_rcv_rst_cfm ( int16_t suId, Rst *rstEvnt, int16_t dChan, uint8_t ces, uint8_t evtType)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_INFO, "Received RESTART CFM suId:%d dChan:%d ces:%d\n", suId, dChan, ces);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_phy_ind(SuId suId, Reason reason)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
    ftdm_log(FTDM_LOG_INFO, "[SNGISDN PHY] D-chan %d : %s\n", suId, DECODE_LL1_REASON(reason));
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
} 

void sngisdn_rcv_q921_ind(BdMngmt *status)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	unsigned j,k;
	ftdm_span_t *ftdmspan = NULL;

	for(j=1;j<=g_sngisdn_data.num_dchan;j++) {
		for(k=1;k<=g_sngisdn_data.dchans[j].num_spans;k++) {
			if (g_sngisdn_data.dchans[j].spans[k]->link_id == status->t.usta.lnkNmb) {
				ftdmspan = (ftdm_span_t*)g_sngisdn_data.dchans[j].spans[k]->ftdm_span;
			}
		}
	}
	if (ftdmspan == NULL) {
		ftdm_log(FTDM_LOG_CRIT, "Received q921 status on unconfigured span\n", status->t.usta.lnkNmb);
#ifdef DEBUG_MODE
		FORCE_SEGFAULT
#endif
		return;
	}

	switch (status->t.usta.alarm.category) {
		case (LCM_CATEGORY_INTERFACE):
			ftdm_log(FTDM_LOG_INFO, "[SNGISDN Q921] %s: %s: %s(%d): %s(%d)\n",
							ftdmspan->name,
							DECODE_LCM_CATEGORY(status->t.usta.alarm.category),
							DECODE_LCM_EVENT(status->t.usta.alarm.event), status->t.usta.alarm.event,
							DECODE_LCM_CAUSE(status->t.usta.alarm.cause), status->t.usta.alarm.cause);
			break;
		default:
			ftdm_log(FTDM_LOG_INFO, "[SNGISDN Q921] %s: %s: %s(%d): %s(%d)\n",
					ftdmspan->name,
					DECODE_LCM_CATEGORY(status->t.usta.alarm.category),
					DECODE_LLD_EVENT(status->t.usta.alarm.event), status->t.usta.alarm.event,
					DECODE_LLD_CAUSE(status->t.usta.alarm.cause), status->t.usta.alarm.cause);
			break;
	}
	
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__)
    return;
}
void sngisdn_rcv_q931_ind(InMngmt *status)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);

	

	ftdm_span_t *ftdmspan = NULL;
	
	if (status->t.usta.alarm.cause == 287) {
		get_memory_info();
		return;
	}

	switch (status->t.usta.alarm.category) {
		case (LCM_CATEGORY_INTERFACE):
			ftdm_log(FTDM_LOG_WARNING, "[SNGISDN Q931] s%d: %s: %s(%d): %s(%d)\n",
							status->t.usta.suId,
							DECODE_LCM_CATEGORY(status->t.usta.alarm.category),
							DECODE_LCM_EVENT(status->t.usta.alarm.event), status->t.usta.alarm.event,
							DECODE_LCM_CAUSE(status->t.usta.alarm.cause), status->t.usta.alarm.cause);

			/* clean this up later */

			switch (status->t.usta.alarm.event) {
				case LCM_EVENT_UP:
				case LCM_EVENT_DOWN:
					{
						unsigned j,k;
						for(j=1;j<=g_sngisdn_data.num_dchan;j++) {
							for(k=1;k<=g_sngisdn_data.dchans[j].num_spans;k++) {
								if (g_sngisdn_data.dchans[j].spans[k]->link_id == status->t.usta.suId) {
									ftdmspan = (ftdm_span_t*)g_sngisdn_data.dchans[j].spans[k]->ftdm_span;
								}
							}
						}

						if (ftdmspan == NULL) {
							ftdm_log(FTDM_LOG_CRIT, "Received q931 LCM EVENT on unconfigured span (suId:%d)\n", status->t.usta.suId);
							return;
						}

						sngisdn_set_span_sig_status(ftdmspan, (status->t.usta.alarm.event == LCM_EVENT_UP)?FTDM_SIG_STATE_UP:FTDM_SIG_STATE_DOWN);
					}
					break;
			}
			break;
		default:
			ftdm_log(FTDM_LOG_DEBUG, "[SNGISDN Q931] s%d: %s: %s(%d): %s(%d)\n",
					status->t.usta.suId,
					DECODE_LCM_CATEGORY(status->t.usta.alarm.category),
					DECODE_LCM_EVENT(status->t.usta.alarm.event), status->t.usta.alarm.event,
					DECODE_LCM_CAUSE(status->t.usta.alarm.cause), status->t.usta.alarm.cause);
			break;
	}


	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
	return;
}

void sngisdn_rcv_cc_ind(CcMngmt *status)
{
	ISDN_FUNC_TRACE_ENTER(__FUNCTION__);
	ftdm_log(FTDM_LOG_INFO, "RECEIVED %s\n", __FUNCTION__);
	ISDN_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

#define Q931_TRC_EVENT(event) (event == TL3PKTTX)?"TX": \
								(event == TL3PKTRX)?"RX":"UNKNOWN"

void sngisdn_rcv_q931_trace(InMngmt *trc, Buffer *mBuf)
{
	MsgLen mlen;
	MsgLen i;
	int16_t j;
	Buffer *tmp;
	Data *cptr;
	uint8_t data;
	uint8_t tdata[1000];
	char *data_str = ftdm_calloc(1,MAX_DECODE_STR_LEN); /* TODO Find a proper size */
	
	ftdm_assert(mBuf != NULLP, "Received a Q931 trace with no buffer");
	mlen = ((SsMsgInfo*)(mBuf->b_rptr))->len;

	if (mlen != 0) {
		tmp = mBuf->b_cont;
		cptr = tmp->b_rptr;
		data = *cptr++;
		i = 0;

		for(j=0;j<mlen;j++) {
			tdata[j]= data;

			if (cptr == tmp->b_wptr) {
				tmp = tmp->b_cont;
				if (tmp) cptr = tmp->b_rptr;
			}
			data = *cptr++;
		}

		sngisdn_trace_q931(data_str, tdata, mlen);
		ftdm_log(FTDM_LOG_INFO, "[SNGISDN Q931] FRAME %s:%s\n", Q931_TRC_EVENT(trc->t.trc.evnt), data_str);
	}

	ftdm_safe_free(data_str);
	/* We do not need to free mBuf in this case because stack does it */
	/* SPutMsg(mBuf); */
	return;
}


#define Q921_TRC_EVENT(event) (event == TL2FRMRX)?"RX": \
								(event == TL2FRMTX)?"TX": \
								(event == TL2TMR)?"TMR EXPIRED":"UNKNOWN"

void sngisdn_rcv_q921_trace(BdMngmt *trc, Buffer *mBuf)
{
	MsgLen mlen;
	MsgLen i;
	int16_t j;
	Buffer *tmp;
	Data *cptr;
	uint8_t data;
	uint8_t tdata[16];
	char *data_str = ftdm_calloc(1,200); /* TODO Find a proper size */
	

	if (trc->t.trc.evnt == TL2TMR) {
		goto end_of_trace;
	}

	ftdm_assert(mBuf != NULLP, "Received a Q921 trace with no buffer");
	mlen = ((SsMsgInfo*)(mBuf->b_rptr))->len;

	if (mlen != 0) {
		tmp = mBuf->b_cont;
		cptr = tmp->b_rptr;
		data = *cptr++;
		i = 0;
		while (i < mlen) {
			j = 0;
			for(j=0;j<16;j++) {
				if (i<mlen) {
					tdata[j]= data;
				
					if (cptr == tmp->b_wptr) {
						tmp = tmp->b_cont;
						if (tmp) cptr = tmp->b_rptr;
					}
					i++;
					if (i<mlen) data = *cptr++;
				}
			}

		}
		sngisdn_trace_q921(data_str, tdata, mlen);
		ftdm_log(FTDM_LOG_INFO, "[SNGISDN Q921] FRAME %s:%s\n", Q921_TRC_EVENT(trc->t.trc.evnt), data_str);
	}
end_of_trace:
	ftdm_safe_free(data_str);
	SPutMsg(mBuf);
	return;
}



void sngisdn_rcv_sng_log(uint8_t level, char *fmt,...)
{
	char    *data;
    int     ret;
    va_list ap;

    va_start(ap, fmt);
    ret = ftdm_vasprintf(&data, fmt, ap);
    if (ret == -1) {
        return;
    }

    switch (level) {
		case SNG_LOGLEVEL_DEBUG:
			ftdm_log(FTDM_LOG_DEBUG, "sng_isdn->%s", data);
			break;
		case SNG_LOGLEVEL_WARN:
			ftdm_log(FTDM_LOG_INFO, "sng_isdn->%s", data);
			break;
		case SNG_LOGLEVEL_INFO:
			ftdm_log(FTDM_LOG_INFO, "sng_isdn->%s", data);
			break;
		case SNG_LOGLEVEL_STATS:
			ftdm_log(FTDM_LOG_INFO, "sng_isdn->%s", data);
			break;
		case SNG_LOGLEVEL_ERROR:
			ftdm_log(FTDM_LOG_ERROR, "sng_isdn->%s", data);
#ifdef DEBUG_MODE
			FORCE_SEGFAULT
#endif
			break;
		case SNG_LOGLEVEL_CRIT:
   			ftdm_log(FTDM_LOG_CRIT, "sng_isdn->%s", data);
#ifdef DEBUG_MODE
			FORCE_SEGFAULT
#endif
			break;
		default:
			ftdm_log(FTDM_LOG_INFO, "sng_isdn->%s", data);
			break;
    }
	return;
}

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
