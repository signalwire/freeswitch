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

ftdm_status_t sngisdn_check_free_ids(void);

extern ftdm_sngisdn_data_t	g_sngisdn_data;
void get_memory_info(void);

void clear_call_data(sngisdn_chan_data_t *sngisdn_info)
{
	uint32_t cc_id = ((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->cc_id;

	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_DEBUG, "Clearing call data (suId:%u suInstId:%u spInstId:%u)\n", cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);
	ftdm_mutex_lock(g_sngisdn_data.ccs[cc_id].mutex);
	g_sngisdn_data.ccs[cc_id].active_spInstIds[sngisdn_info->spInstId]=NULL;
	g_sngisdn_data.ccs[cc_id].active_suInstIds[sngisdn_info->suInstId]=NULL;
	ftdm_mutex_unlock(g_sngisdn_data.ccs[cc_id].mutex);
	
	sngisdn_info->suInstId = 0;
	sngisdn_info->spInstId = 0;
	sngisdn_info->globalFlg = 0;
	sngisdn_info->flags = 0;
	return;
}

void clear_call_glare_data(sngisdn_chan_data_t *sngisdn_info)
{
	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_DEBUG, "Clearing glare data (suId:%d suInstId:%u spInstId:%u actv-suInstId:%u  actv-spInstId:%u)\n",
										sngisdn_info->glare.suId,
										sngisdn_info->glare.suInstId, sngisdn_info->glare.spInstId,
										sngisdn_info->suInstId, sngisdn_info->spInstId);

	ftdm_mutex_lock(g_sngisdn_data.ccs[sngisdn_info->glare.suId].mutex);	
	if (sngisdn_info->glare.spInstId != sngisdn_info->spInstId) {
		g_sngisdn_data.ccs[sngisdn_info->glare.suId].active_spInstIds[sngisdn_info->glare.spInstId]=NULL;
	}
	g_sngisdn_data.ccs[sngisdn_info->glare.suId].active_suInstIds[sngisdn_info->glare.suInstId]=NULL;
	ftdm_mutex_unlock(g_sngisdn_data.ccs[sngisdn_info->glare.suId].mutex);

	ftdm_clear_flag(sngisdn_info, FLAG_GLARE);
	memset(&sngisdn_info->glare.setup, 0, sizeof(ConEvnt));
	sngisdn_info->glare.suId = 0;
	sngisdn_info->glare.suInstId = 0;
	sngisdn_info->glare.spInstId = 0;
	sngisdn_info->glare.dChan = 0;
	sngisdn_info->glare.ces = 0;
	return;
}


uint32_t get_unique_suInstId(int16_t cc_id)
{
	uint32_t suInstId;
	ftdm_assert_return((cc_id > 0 && cc_id <=MAX_VARIANTS), FTDM_FAIL, "Invalid cc_id\n");
	ftdm_mutex_lock(g_sngisdn_data.ccs[cc_id].mutex);
	suInstId = g_sngisdn_data.ccs[cc_id].last_suInstId;

	while(1) {
		if (++suInstId == MAX_INSTID) {
			suInstId = 1;
		}
		if (g_sngisdn_data.ccs[cc_id].active_suInstIds[suInstId] == NULL) {
			g_sngisdn_data.ccs[cc_id].last_suInstId = suInstId;
			ftdm_mutex_unlock(g_sngisdn_data.ccs[cc_id].mutex);
			return suInstId;
		}
	}
	/* Should never reach here */
	ftdm_mutex_unlock(g_sngisdn_data.ccs[cc_id].mutex);
	return 0;
}

ftdm_status_t get_ftdmchan_by_suInstId(int16_t cc_id, uint32_t suInstId, sngisdn_chan_data_t **sngisdn_data)
{
	ftdm_assert_return((cc_id > 0 && cc_id <=MAX_VARIANTS), FTDM_FAIL, "Invalid cc_id\n");
	ftdm_assert_return(g_sngisdn_data.ccs[cc_id].activation_done, FTDM_FAIL, "Trying to find call on unconfigured CC\n");

	if (g_sngisdn_data.ccs[cc_id].active_suInstIds[suInstId] == NULL) {
		return FTDM_FAIL;
	}
	*sngisdn_data = g_sngisdn_data.ccs[cc_id].active_suInstIds[suInstId];	
	return FTDM_SUCCESS;
}

ftdm_status_t get_ftdmchan_by_spInstId(int16_t cc_id, uint32_t spInstId, sngisdn_chan_data_t **sngisdn_data)
{
	ftdm_assert_return((cc_id > 0 && cc_id <=MAX_VARIANTS), FTDM_FAIL, "Invalid cc_id\n");
	ftdm_assert_return(g_sngisdn_data.ccs[cc_id].activation_done, FTDM_FAIL, "Trying to find call on unconfigured CC\n");

	if (g_sngisdn_data.ccs[cc_id].active_spInstIds[spInstId] == NULL) {
		return FTDM_FAIL;
	}
	*sngisdn_data = g_sngisdn_data.ccs[cc_id].active_spInstIds[spInstId];
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_set_avail_rate(ftdm_span_t *span, sngisdn_avail_t avail)
{
	
	if (span->trunk_type == FTDM_TRUNK_BRI ||
		span->trunk_type == FTDM_TRUNK_BRI_PTMP) {

		ftdm_iterator_t *chaniter = NULL;
		ftdm_iterator_t *curr = NULL;


		chaniter = ftdm_span_get_chan_iterator(span, NULL);
		for (curr = chaniter; curr; curr = ftdm_iterator_next(curr)) {
			ftdm_log_chan(((ftdm_channel_t*)ftdm_iterator_current(curr)), FTDM_LOG_DEBUG, "Setting availability rate to:%d\n", avail);
			((ftdm_channel_t*)ftdm_iterator_current(curr))->availability_rate = avail;
		}
		ftdm_iterator_free(chaniter);
	}
	return FTDM_SUCCESS;
}

ftdm_status_t get_calling_num(ftdm_caller_data_t *caller_data, CgPtyNmb *cgPtyNmb)
{
	if (cgPtyNmb->eh.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}

	if (cgPtyNmb->screenInd.pres == PRSNT_NODEF) {
		caller_data->screen = cgPtyNmb->screenInd.val;
	}

	if (cgPtyNmb->presInd0.pres == PRSNT_NODEF) {
		caller_data->pres = cgPtyNmb->presInd0.val;
	}

	if (cgPtyNmb->nmbPlanId.pres == PRSNT_NODEF) {
		caller_data->cid_num.plan = cgPtyNmb->nmbPlanId.val;
	}
		
	if (cgPtyNmb->typeNmb1.pres == PRSNT_NODEF) {
		caller_data->cid_num.type = cgPtyNmb->typeNmb1.val;
	}
	
	if (cgPtyNmb->nmbDigits.pres == PRSNT_NODEF) {
		ftdm_copy_string(caller_data->cid_num.digits, (const char*)cgPtyNmb->nmbDigits.val, cgPtyNmb->nmbDigits.len+1);
	}
	memcpy(&caller_data->ani, &caller_data->cid_num, sizeof(caller_data->ani));
	return FTDM_SUCCESS;
}

ftdm_status_t get_called_num(ftdm_caller_data_t *caller_data, CdPtyNmb *cdPtyNmb)
{
	if (cdPtyNmb->eh.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}

	if (cdPtyNmb->nmbPlanId.pres == PRSNT_NODEF) {
		caller_data->dnis.plan = cdPtyNmb->nmbPlanId.val;
	}

	if (cdPtyNmb->typeNmb0.pres == PRSNT_NODEF) {
		caller_data->dnis.type = cdPtyNmb->typeNmb0.val;
	}
	
	if (cdPtyNmb->nmbDigits.pres == PRSNT_NODEF) {
		/* In overlap receive mode, append the new digits to the existing dnis */
		unsigned i = strlen(caller_data->dnis.digits);
		
		ftdm_copy_string(&caller_data->dnis.digits[i], (const char*)cdPtyNmb->nmbDigits.val, cdPtyNmb->nmbDigits.len+1);
	}
	return FTDM_SUCCESS;
}

ftdm_status_t get_redir_num(ftdm_caller_data_t *caller_data, RedirNmb *redirNmb)
{
	if (redirNmb->eh.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}

	if (redirNmb->nmbPlanId.pres == PRSNT_NODEF) {
		caller_data->rdnis.plan = redirNmb->nmbPlanId.val;
	}

	if (redirNmb->typeNmb.pres == PRSNT_NODEF) {
		caller_data->rdnis.type = redirNmb->typeNmb.val;
	}
	
	if (redirNmb->nmbDigits.pres == PRSNT_NODEF) {
		ftdm_copy_string(caller_data->rdnis.digits, (const char*)redirNmb->nmbDigits.val, redirNmb->nmbDigits.len+1);
	}
	return FTDM_SUCCESS;
}

ftdm_status_t get_calling_name_from_display(ftdm_caller_data_t *caller_data, Display *display)
{
	if (display->eh.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}
	if (display->dispInfo.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}
	
	ftdm_copy_string(caller_data->cid_name, (const char*)display->dispInfo.val, display->dispInfo.len+1);
	return FTDM_SUCCESS;
}

ftdm_status_t get_calling_name_from_usr_usr(ftdm_caller_data_t *caller_data, UsrUsr *usrUsr)
{
	if (usrUsr->eh.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}
	
	if (usrUsr->protocolDisc.val != PD_IA5) {
		return FTDM_FAIL;
	}
		
	if (usrUsr->usrInfo.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}
		
	ftdm_copy_string(caller_data->cid_name, (const char*)usrUsr->usrInfo.val, usrUsr->usrInfo.len+1);
	return FTDM_SUCCESS;
}

ftdm_status_t get_facility_ie(ftdm_caller_data_t *caller_data, uint8_t *data, uint32_t data_len)
{
	if (data_len > sizeof(caller_data->raw_data)-2) {
		ftdm_log(FTDM_LOG_CRIT, "Length of Facility IE exceeds maximum length\n");
		return FTDM_FAIL;
	}
	
	memset(caller_data->raw_data, 0, sizeof(caller_data->raw_data));
	/* Always include Facility IE identifier + len so this can be used as a sanity check by the user */
	caller_data->raw_data[0] = 0x1C;
	caller_data->raw_data[1] = data_len;
	
	memcpy(&caller_data->raw_data[2], data, data_len);
	caller_data->raw_data_len = data_len+2;
	
	return FTDM_SUCCESS;
}

ftdm_status_t set_calling_num(CgPtyNmb *cgPtyNmb, ftdm_caller_data_t *caller_data)
{
	uint8_t len = strlen(caller_data->cid_num.digits);
	if (!len) {
		return FTDM_SUCCESS;
	}
	cgPtyNmb->eh.pres			= PRSNT_NODEF;

	cgPtyNmb->screenInd.pres	= PRSNT_NODEF;
	cgPtyNmb->screenInd.val		= caller_data->screen;

	cgPtyNmb->presInd0.pres     = PRSNT_NODEF;
	cgPtyNmb->presInd0.val      = caller_data->pres;
	
	cgPtyNmb->nmbPlanId.pres	= PRSNT_NODEF;
	cgPtyNmb->nmbPlanId.val		= caller_data->cid_num.plan;

	cgPtyNmb->typeNmb1.pres		= PRSNT_NODEF;
	cgPtyNmb->typeNmb1.val		= caller_data->cid_num.type;

	cgPtyNmb->nmbDigits.pres	= PRSNT_NODEF;
	cgPtyNmb->nmbDigits.len		= len;

	memcpy(cgPtyNmb->nmbDigits.val, caller_data->cid_num.digits, len);

	return FTDM_SUCCESS;
}

ftdm_status_t set_called_num(CdPtyNmb *cdPtyNmb, ftdm_caller_data_t *caller_data)
{
	uint8_t len = strlen(caller_data->dnis.digits);
	if (!len) {
		return FTDM_SUCCESS;
	}
	cdPtyNmb->eh.pres           = PRSNT_NODEF;

	cdPtyNmb->nmbPlanId.pres      = PRSNT_NODEF;
	if (caller_data->dnis.plan == FTDM_NPI_INVALID) {
		cdPtyNmb->nmbPlanId.val       = FTDM_NPI_UNKNOWN;
	} else {
		cdPtyNmb->nmbPlanId.val       = caller_data->dnis.plan;
	}
	
	cdPtyNmb->typeNmb0.pres       = PRSNT_NODEF;
	if (caller_data->dnis.type == FTDM_TON_INVALID) {
		cdPtyNmb->typeNmb0.val        = FTDM_TON_UNKNOWN;
	} else {
		cdPtyNmb->typeNmb0.val        = caller_data->dnis.type;
	}

	cdPtyNmb->nmbDigits.pres = PRSNT_NODEF;
	cdPtyNmb->nmbDigits.len = len;

	
	memcpy(cdPtyNmb->nmbDigits.val, caller_data->dnis.digits, len);
    return FTDM_SUCCESS;
}

ftdm_status_t set_redir_num(RedirNmb *redirNmb, ftdm_caller_data_t *caller_data)
{
	uint8_t len = strlen(caller_data->rdnis.digits);
	if (!len) {
		return FTDM_SUCCESS;
	}

	redirNmb->eh.pres 	= PRSNT_NODEF;

	redirNmb->nmbPlanId.pres 	= PRSNT_NODEF;
	if (caller_data->rdnis.plan == FTDM_NPI_INVALID) {
		redirNmb->nmbPlanId.val	= FTDM_NPI_UNKNOWN;
	} else {
		redirNmb->nmbPlanId.val = caller_data->rdnis.plan;
	}

	redirNmb->typeNmb.pres		= PRSNT_NODEF;
	if (caller_data->rdnis.type == FTDM_TON_INVALID) {
		redirNmb->typeNmb.val		= FTDM_TON_UNKNOWN;
	} else {
		redirNmb->typeNmb.val		= caller_data->rdnis.type;
	}

	redirNmb->nmbDigits.pres = PRSNT_NODEF;
	redirNmb->nmbDigits.len = len;

	memcpy(redirNmb->nmbDigits.val, caller_data->rdnis.digits, len);

	return FTDM_SUCCESS;
}


ftdm_status_t set_calling_name(ConEvnt *conEvnt, ftdm_channel_t *ftdmchan)
{
	uint8_t len;
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
	/* sngisdn_chan_data_t *sngisdn_info = ftdmchan->call_data; */
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	len = strlen(caller_data->cid_name);
	if (!len) {
		return FTDM_SUCCESS;
	}

	if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI ||
		ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP) {

		conEvnt->usrUsr.eh.pres = PRSNT_NODEF;
		conEvnt->usrUsr.protocolDisc.pres = PRSNT_NODEF;
		conEvnt->usrUsr.protocolDisc.val = PD_IA5; /* IA5 chars */
		conEvnt->usrUsr.usrInfo.pres = PRSNT_NODEF;
		conEvnt->usrUsr.usrInfo.len = len;
		/* in sangoma_brid we used to send usr-usr info as <cid_name>!<calling_number>,
		change to previous style if current one does not work */
		memcpy(conEvnt->usrUsr.usrInfo.val, caller_data->cid_name, len);
	} else {
		switch (signal_data->switchtype) {
		case SNGISDN_SWITCH_NI2:
			/* TODO: Need to send the caller ID as a facility IE */

			break;
		case SNGISDN_SWITCH_EUROISDN:
			if (signal_data->signalling != SNGISDN_SIGNALING_NET) {
			break;
			}
			/* follow through */
		case SNGISDN_SWITCH_5ESS:
		case SNGISDN_SWITCH_4ESS:
		case SNGISDN_SWITCH_DMS100:
			conEvnt->display.eh.pres = PRSNT_NODEF;
			conEvnt->display.dispInfo.pres = PRSNT_NODEF;
			conEvnt->display.dispInfo.len = len;
			memcpy(conEvnt->display.dispInfo.val, caller_data->cid_name, len);
			break;
		case SNGISDN_SWITCH_QSIG:
			/* It seems like QSIG does not support Caller ID Name */
			break;
		case SNGISDN_SWITCH_INSNET:
			/* Don't know how to transmit caller ID name on INSNET */
			break;
		}
	}
	return FTDM_SUCCESS;
}

ftdm_status_t set_facility_ie(ftdm_channel_t *ftdmchan, FacilityStr *facilityStr)
{
	const char *facility_str = NULL;
	
	facility_str = ftdm_channel_get_var(ftdmchan, "isdn.facility.val");
	if (facility_str) {
		facilityStr->eh.pres = PRSNT_NODEF;
		facilityStr->facilityStr.len = strlen(facility_str);
		memcpy(facilityStr->facilityStr.val, facility_str, facilityStr->facilityStr.len);
		return FTDM_SUCCESS;
	}
	return FTDM_FAIL;
}

void sngisdn_t3_timeout(void* p_sngisdn_info)
{
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*)p_sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Timer T3 expired (suId:%d suInstId:%u spInstId:%u)\n",
				  signal_data->cc_id, sngisdn_info->glare.spInstId, sngisdn_info->glare.suInstId);
	ftdm_mutex_lock(ftdmchan->mutex);
	if (ftdm_test_flag(sngisdn_info, FLAG_ACTIVATING)){
		/* PHY layer timed-out, need to clear the call */
		ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Failed to Wake-Up line (suId:%d suInstId:%u spInstId:%u)\n",
					  signal_data->cc_id, sngisdn_info->glare.spInstId, sngisdn_info->glare.suInstId);

		ftdmchan->caller_data.hangup_cause = FTDM_CAUSE_NO_ROUTE_DESTINATION;
		ftdm_clear_flag(sngisdn_info, FLAG_ACTIVATING);
		ftdm_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
		ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
	}	
	ftdm_mutex_unlock(ftdmchan->mutex);
}

void sngisdn_delayed_setup(void* p_sngisdn_info)
{
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*)p_sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;

	ftdm_mutex_lock(ftdmchan->mutex);
	sngisdn_snd_setup(ftdmchan);
	ftdm_mutex_unlock(ftdmchan->mutex);
	return;
}

void sngisdn_delayed_release(void* p_sngisdn_info)
{
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*)p_sngisdn_info;	
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;	

	ftdm_mutex_lock(ftdmchan->mutex);
	
	if (ftdm_test_flag(sngisdn_info, FLAG_DELAYED_REL)) {
		ftdm_clear_flag(sngisdn_info, FLAG_DELAYED_REL);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Sending delayed RELEASE (suId:%d suInstId:%u spInstId:%u)\n",
								signal_data->cc_id, sngisdn_info->glare.spInstId, sngisdn_info->glare.suInstId);

		sngisdn_snd_release(ftdmchan, 1);
		clear_call_glare_data(sngisdn_info);
	} else {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Call was already released (suId:%d suInstId:%u spInstId:%u)\n",
								signal_data->cc_id, sngisdn_info->glare.spInstId, sngisdn_info->glare.suInstId);
	}
	ftdm_mutex_unlock(ftdmchan->mutex);
	return;
}

void sngisdn_delayed_connect(void* p_sngisdn_info)
{
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*)p_sngisdn_info;	
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;	

	ftdm_mutex_lock(ftdmchan->mutex);
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Sending delayed CONNECT (suId:%d suInstId:%u spInstId:%u)\n",
								signal_data->cc_id, sngisdn_info->glare.spInstId, sngisdn_info->glare.suInstId);

	sngisdn_snd_connect(ftdmchan);
	ftdm_mutex_unlock(ftdmchan->mutex);
	return;
}

void sngisdn_delayed_disconnect(void* p_sngisdn_info)
{
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*)p_sngisdn_info;	
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;	

	ftdm_mutex_lock(ftdmchan->mutex);
	if (ftdmchan->caller_data.hangup_cause == IN_CCNORTTODEST || ftdmchan->state != FTDM_CHANNEL_STATE_DOWN) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Sending delayed DISCONNECT (suId:%d suInstId:%u spInstId:%u)\n",
									signal_data->cc_id, sngisdn_info->glare.spInstId, sngisdn_info->glare.suInstId);

 		sngisdn_snd_disconnect(ftdmchan);
		if (ftdmchan->caller_data.hangup_cause == IN_CCNORTTODEST) {
			ftdm_channel_t *close_chan = ftdmchan;
			ftdm_channel_close(&close_chan);
		}
	}

	ftdm_mutex_unlock(ftdmchan->mutex);
	return;
}

void sngisdn_facility_timeout(void* p_sngisdn_info)
{
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*)p_sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;	

	ftdm_mutex_lock(ftdmchan->mutex);
	if (ftdmchan->state == FTDM_CHANNEL_STATE_GET_CALLERID) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Facility timeout reached proceeding with call (suId:%d suInstId:%u spInstId:%u)\n",
					  signal_data->cc_id, sngisdn_info->spInstId, sngisdn_info->suInstId);
		
		ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RING);
	}
	
	ftdm_mutex_unlock(ftdmchan->mutex);
	return;
}

ftdm_status_t sngisdn_check_free_ids(void)
{
	unsigned i;
	unsigned j;
	ftdm_log(FTDM_LOG_INFO, "Checking suInstId's\n");
	for(j=1;j<=MAX_VARIANTS;j++) {
		if (g_sngisdn_data.ccs[j].config_done) {
			for(i=1;i<MAX_INSTID;i++) {
				if (g_sngisdn_data.ccs[j].active_suInstIds[i] != NULL) {
					ftdm_log(FTDM_LOG_INFO, "suId:%u suInstId:%u is not free\n", j, i);
					

				}
			}
		}
	}

	ftdm_log(FTDM_LOG_INFO, "Checking spInstId's\n");
	for(j=1;j<=MAX_VARIANTS;j++) {
		if (g_sngisdn_data.ccs[j].config_done) {
			for(i=1;i<MAX_INSTID;i++) {
				if (g_sngisdn_data.ccs[j].active_spInstIds[i] != NULL) {
					ftdm_log(FTDM_LOG_INFO, "suId:%u spInstId:%u is not free\n", j, i);
					

				}
			}
		}
	}
	ftdm_log(FTDM_LOG_INFO, "Checking ID's done\n");
	return FTDM_SUCCESS;
}

void get_memory_info(void)
{
	U32 availmen = 0;
	SRegInfoShow(S_REG, &availmen);
	return;
}

uint8_t sngisdn_get_infoTranCap_from_stack(ftdm_bearer_cap_t bearer_capability)
{
	switch(bearer_capability) {
	case FTDM_BEARER_CAP_SPEECH:
		return IN_ITC_SPEECH;

	case FTDM_BEARER_CAP_64K_UNRESTRICTED:
		return IN_ITC_UNRDIG;

	case FTDM_BEARER_CAP_3_1KHZ_AUDIO:
		return IN_ITC_A31KHZ;
		
		/* Do not put a default case here, so we can see compile warnings if we have unhandled cases */
	}
	return FTDM_BEARER_CAP_SPEECH;
}

uint8_t sngisdn_get_usrInfoLyr1Prot_from_stack(ftdm_user_layer1_prot_t layer1_prot)
{
	switch(layer1_prot) {
	case FTDM_USER_LAYER1_PROT_V110:
		return IN_UIL1_CCITTV110;

	case FTDM_USER_LAYER1_PROT_ULAW:
		return IN_UIL1_G711ULAW;

	case FTDM_USER_LAYER1_PROT_ALAW:
		return IN_UIL1_G711ALAW;
			
	/* Do not put a default case here, so we can see compile warnings if we have unhandled cases */
	}
	return IN_UIL1_G711ULAW;
}

ftdm_bearer_cap_t sngisdn_get_infoTranCap_from_user(uint8_t bearer_capability)
{
	switch(bearer_capability) {
	case IN_ITC_SPEECH:
		return FTDM_BEARER_CAP_SPEECH;
		
	case IN_ITC_UNRDIG:
		return FTDM_BEARER_CAP_64K_UNRESTRICTED;
		
	case IN_ITC_A31KHZ:
		return FTDM_BEARER_CAP_3_1KHZ_AUDIO;

	default:
		return FTDM_BEARER_CAP_SPEECH;
	}
	return FTDM_BEARER_CAP_SPEECH;
}

ftdm_user_layer1_prot_t sngisdn_get_usrInfoLyr1Prot_from_user(uint8_t layer1_prot)
{
	switch(layer1_prot) {
	case IN_UIL1_CCITTV110:
		return FTDM_USER_LAYER1_PROT_V110;
	case IN_UIL1_G711ULAW:
		return FTDM_USER_LAYER1_PROT_ULAW;
	case IN_UIL1_G711ALAW:
		return IN_UIL1_G711ALAW;
	default:
		return FTDM_USER_LAYER1_PROT_ULAW;
	}
	return FTDM_USER_LAYER1_PROT_ULAW;
}

void sngisdn_print_phy_stats(ftdm_stream_handle_t *stream, ftdm_span_t *span)
{
	L1Mngmt sts;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;

	memset(&sts, 0, sizeof(sts));
	sng_isdn_phy_stats(signal_data->link_id , &sts);

	stream->write_function(stream, "\n---------------------------------------------------------------------\n");
	stream->write_function(stream, "   Span:%s", span->name);
	stream->write_function(stream, "\n---------------------------------------------------------------------\n");
	stream->write_function(stream, "   Performance Counters");
	stream->write_function(stream, "\n---------------------------------------------------------------------\n");
	stream->write_function(stream, "RX Packets:\t%u\tTX Packets:\t%u\tEvents:%u\n", sts.t.sts.rx_packets, sts.t.sts.tx_packets, sts.t.sts.rx_events);
	stream->write_function(stream, "RX Bytes:\t%u\tTX Bytes:\t%u\n\n", sts.t.sts.rx_bytes, sts.t.sts.tx_bytes);
	stream->write_function(stream, "TX Queue:\t%u/%u\tRX Queue:\t%u/%u\tEvents Queue:\t%u/%u\n",
							sts.t.sts.num_frames_in_tx_queue,sts.t.sts.tx_queue_len,
							sts.t.sts.num_frames_in_rx_queue, sts.t.sts.rx_queue_len,
							sts.t.sts.rx_events_in_queue, sts.t.sts.event_queue_len);
	
	stream->write_function(stream, "\n---------------------------------------------------------------------\n");
	stream->write_function(stream, "   Errors");
	stream->write_function(stream, "\n---------------------------------------------------------------------\n");
	stream->write_function(stream, "RX Errors:\t%u\tTX Errors:\t%u\n", sts.t.sts.rx_errors, sts.t.sts.tx_errors);
	stream->write_function(stream, "RX Dropped:\t%u\tTX Dropped:\t%u\tEvents Dropped:\t%u\n", sts.t.sts.rx_dropped, sts.t.sts.tx_dropped,sts.t.sts.rx_events_dropped);


	stream->write_function(stream, "\n---------------------------------------------------------------------\n");
	stream->write_function(stream, "   RX Errors Details");
	stream->write_function(stream, "\n---------------------------------------------------------------------\n");
	stream->write_function(stream, "CRC:\t\t%u\tFrame:\t\t%u\tOverruns:\t%u\n", sts.t.sts.rx_crc_errors, sts.t.sts.rx_frame_errors, sts.t.sts.rx_over_errors);
	stream->write_function(stream, "Fifo:\t\t%u\tAborts:\t\t%u\tMissed:\t\t%u\n", sts.t.sts.rx_fifo_errors, sts.t.sts.rx_hdlc_abort_counter, sts.t.sts.rx_missed_errors);
	stream->write_function(stream, "Length:\t\t%u\n", sts.t.sts.rx_length_errors);

	stream->write_function(stream, "\n---------------------------------------------------------------------\n");
	stream->write_function(stream, "   TX Errors Details");
	stream->write_function(stream, "\n---------------------------------------------------------------------\n");
	stream->write_function(stream, "Aborted:\t%u\tFifo:\t\t%u\tCarrier:\t%u\n", sts.t.sts.tx_aborted_errors, sts.t.sts.tx_fifo_errors, sts.t.sts.tx_carrier_errors);
	return;
}


void sngisdn_print_span(ftdm_stream_handle_t *stream, ftdm_span_t *span)
{
	ftdm_signaling_status_t sigstatus;
	ftdm_alarm_flag_t alarmbits;
	ftdm_channel_t *fchan;
	alarmbits = FTDM_ALARM_NONE;
	fchan = ftdm_span_get_channel(span, 1);
	if (fchan) {
		ftdm_channel_get_alarms(fchan, &alarmbits);
	}
			
	ftdm_span_get_sig_status(span, &sigstatus);
	stream->write_function(stream, "span:%s physical:%s signalling:%s\n",
										span->name, alarmbits ? "ALARMED" : "OK",
										ftdm_signaling_status2str(sigstatus));
	return;
}

void sngisdn_print_spans(ftdm_stream_handle_t *stream)
{
	int i;	
	for(i=1;i<=MAX_L1_LINKS;i++) {		
		if (g_sngisdn_data.spans[i]) {
			sngisdn_print_span(stream, g_sngisdn_data.spans[i]->ftdm_span);
		}
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
