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

ftdm_status_t cpy_calling_num_from_sngisdn(ftdm_caller_data_t *ftdm, CgPtyNmb *cgPtyNmb);
ftdm_status_t cpy_called_num_from_sngisdn(ftdm_caller_data_t *ftdm, CdPtyNmb *cdPtyNmb);
ftdm_status_t cpy_redir_num_from_sngisdn(ftdm_caller_data_t *ftdm, RedirNmb *redirNmb);
ftdm_status_t cpy_calling_name_from_sngisdn(ftdm_caller_data_t *ftdm, ConEvnt *conEvnt);

ftdm_status_t cpy_calling_num_to_sngisdn(CgPtyNmb *cgPtyNmb, ftdm_caller_data_t *ftdm);
ftdm_status_t cpy_called_num_to_sngisdn(CdPtyNmb *cdPtyNmb, ftdm_caller_data_t *ftdm);
ftdm_status_t cpy_redir_num_to_sngisdn(RedirNmb *redirNmb, ftdm_caller_data_t *ftdm);
ftdm_status_t cpy_calling_name_to_sngisdn(ConEvnt *conEvnt, ftdm_channel_t *ftdmchan);

extern ftdm_sngisdn_data_t	g_sngisdn_data;
void get_memory_info(void);

void clear_call_data(sngisdn_chan_data_t *sngisdn_info)
{
	uint32_t cc_id = ((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->cc_id;

	g_sngisdn_data.ccs[cc_id].active_spInstIds[sngisdn_info->spInstId]=NULL;
	g_sngisdn_data.ccs[cc_id].active_suInstIds[sngisdn_info->suInstId]=NULL;
	
	sngisdn_info->suInstId = 0;
	sngisdn_info->spInstId = 0;
	sngisdn_info->globalFlg = 0;
	sngisdn_info->flags = 0;
	return;
}

uint32_t get_unique_suInstId(uint8_t cc_id)
{
	uint32_t suInstId;
	ftdm_mutex_lock(g_sngisdn_data.ccs[cc_id].request_mutex);
	suInstId = g_sngisdn_data.ccs[cc_id].last_suInstId;

	while(1) {
		if (++suInstId == MAX_INSTID) {
			suInstId = 1;
		}
		if (g_sngisdn_data.ccs[cc_id].active_suInstIds[suInstId] == NULL) {
			g_sngisdn_data.ccs[cc_id].last_suInstId = suInstId;
			ftdm_mutex_unlock(g_sngisdn_data.ccs[cc_id].request_mutex);
			return suInstId;
		}
	}
	/* Should never reach here */
	ftdm_mutex_unlock(g_sngisdn_data.ccs[cc_id].request_mutex);
	return 0;
}

ftdm_status_t get_ftdmchan_by_suInstId(uint8_t cc_id, uint32_t suInstId, sngisdn_chan_data_t **sngisdn_data)
{
	ftdm_assert_return(g_sngisdn_data.ccs[cc_id].activation_done, FTDM_FAIL, "Trying to find call on unconfigured CC\n");

	if (g_sngisdn_data.ccs[cc_id].active_suInstIds[suInstId] == NULL) {
		return FTDM_FAIL;
	}
	*sngisdn_data = g_sngisdn_data.ccs[cc_id].active_suInstIds[suInstId];	
	return FTDM_SUCCESS;
}

ftdm_status_t get_ftdmchan_by_spInstId(uint8_t cc_id, uint32_t spInstId, sngisdn_chan_data_t **sngisdn_data)
{
	ftdm_assert_return(g_sngisdn_data.ccs[cc_id].activation_done, FTDM_FAIL, "Trying to find call on unconfigured CC\n");

	if (g_sngisdn_data.ccs[cc_id].active_spInstIds[spInstId] == NULL) {
		return FTDM_FAIL;
	}
	*sngisdn_data = g_sngisdn_data.ccs[cc_id].active_spInstIds[spInstId];
	return FTDM_SUCCESS;
}


ftdm_status_t cpy_calling_num_from_sngisdn(ftdm_caller_data_t *ftdm, CgPtyNmb *cgPtyNmb)
{
	if (cgPtyNmb->eh.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}

	if (cgPtyNmb->screenInd.pres == PRSNT_NODEF) {
		ftdm->screen = cgPtyNmb->screenInd.val;
	}

	if (cgPtyNmb->presInd0.pres == PRSNT_NODEF) {
		ftdm->pres = cgPtyNmb->presInd0.val;
	}

	if (cgPtyNmb->nmbPlanId.pres == PRSNT_NODEF) {
		ftdm->cid_num.plan = cgPtyNmb->nmbPlanId.val;
	}
	if (cgPtyNmb->typeNmb1.pres == PRSNT_NODEF) {
		ftdm->cid_num.type = cgPtyNmb->typeNmb1.val;
	}
	
	if (cgPtyNmb->nmbDigits.pres == PRSNT_NODEF) {
		ftdm_copy_string(ftdm->cid_num.digits, (const char*)cgPtyNmb->nmbDigits.val, cgPtyNmb->nmbDigits.len+1);
	}
	return FTDM_SUCCESS;
}

ftdm_status_t cpy_called_num_from_sngisdn(ftdm_caller_data_t *ftdm, CdPtyNmb *cdPtyNmb)
{
	if (cdPtyNmb->eh.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}

	if (cdPtyNmb->nmbPlanId.pres == PRSNT_NODEF) {
		ftdm->cid_num.plan = cdPtyNmb->nmbPlanId.val;
	}

	if (cdPtyNmb->typeNmb0.pres == PRSNT_NODEF) {
		ftdm->cid_num.type = cdPtyNmb->typeNmb0.val;
	}
	
	if (cdPtyNmb->nmbDigits.pres == PRSNT_NODEF) {
		ftdm_copy_string(ftdm->dnis.digits, (const char*)cdPtyNmb->nmbDigits.val, cdPtyNmb->nmbDigits.len+1);
	}
	return FTDM_SUCCESS;
}

ftdm_status_t cpy_redir_num_from_sngisdn(ftdm_caller_data_t *ftdm, RedirNmb *redirNmb)
{
	if (redirNmb->eh.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}

	if (redirNmb->nmbPlanId.pres == PRSNT_NODEF) {
		ftdm->rdnis.plan = redirNmb->nmbPlanId.val;
	}

	if (redirNmb->typeNmb.pres == PRSNT_NODEF) {
		ftdm->rdnis.type = redirNmb->typeNmb.val;
	}
	
	if (redirNmb->nmbDigits.pres == PRSNT_NODEF) {
		ftdm_copy_string(ftdm->rdnis.digits, (const char*)redirNmb->nmbDigits.val, redirNmb->nmbDigits.len+1);
	}
	return FTDM_SUCCESS;
}

ftdm_status_t cpy_calling_name_from_sngisdn(ftdm_caller_data_t *ftdm, ConEvnt *conEvnt)
{
	if (conEvnt->display.eh.pres && conEvnt->display.dispInfo.pres == PRSNT_NODEF) {
		ftdm_copy_string(ftdm->cid_name, (const char*)conEvnt->display.dispInfo.val, conEvnt->display.dispInfo.len+1);
	}

	/* TODO check if caller name is contained in a Facility IE */
	return FTDM_SUCCESS;
}

ftdm_status_t cpy_calling_num_to_sngisdn(CgPtyNmb *cgPtyNmb, ftdm_caller_data_t *ftdm)
{
	uint8_t len = strlen(ftdm->cid_num.digits);
	if (!len) {
		return FTDM_SUCCESS;
	}
	cgPtyNmb->eh.pres			= PRSNT_NODEF;

	cgPtyNmb->screenInd.pres	= PRSNT_NODEF;
	cgPtyNmb->screenInd.val		= ftdm->screen;

	cgPtyNmb->presInd0.pres     = PRSNT_NODEF;
	cgPtyNmb->presInd0.val      = ftdm->pres;

	cgPtyNmb->nmbPlanId.pres	= PRSNT_NODEF;
	cgPtyNmb->nmbPlanId.val		= ftdm->cid_num.plan;

	cgPtyNmb->typeNmb1.pres		= PRSNT_NODEF;
	cgPtyNmb->typeNmb1.val		= ftdm->cid_num.type;

	cgPtyNmb->nmbDigits.pres	= PRSNT_NODEF;
	cgPtyNmb->nmbDigits.len		= len;

	memcpy(cgPtyNmb->nmbDigits.val, ftdm->cid_num.digits, len);

	return FTDM_SUCCESS;
}

ftdm_status_t cpy_called_num_to_sngisdn(CdPtyNmb *cdPtyNmb, ftdm_caller_data_t *ftdm)
{
	uint8_t len = strlen(ftdm->dnis.digits);
	if (!len) {
		return FTDM_SUCCESS;
	}
	cdPtyNmb->eh.pres           = PRSNT_NODEF;

	cdPtyNmb->nmbPlanId.pres      = PRSNT_NODEF;
	cdPtyNmb->nmbPlanId.val       = ftdm->dnis.plan;

	cdPtyNmb->typeNmb0.pres       = PRSNT_NODEF;
	cdPtyNmb->typeNmb0.val        = ftdm->dnis.type;

	cdPtyNmb->nmbDigits.pres = PRSNT_NODEF;
	cdPtyNmb->nmbDigits.len = len;

	memcpy(cdPtyNmb->nmbDigits.val, ftdm->dnis.digits, len);
    return FTDM_SUCCESS;
}

ftdm_status_t cpy_redir_num_to_sngisdn(RedirNmb *redirNmb, ftdm_caller_data_t *ftdm)
{
	uint8_t len = strlen(ftdm->rdnis.digits);
	if (!len) {
		return FTDM_SUCCESS;
	}

	redirNmb->eh.pres 	= PRSNT_NODEF;

	redirNmb->nmbPlanId.pres 	= PRSNT_NODEF;
	redirNmb->nmbPlanId.val 	= ftdm->rdnis.plan;

	redirNmb->typeNmb.pres		= PRSNT_NODEF;
	redirNmb->typeNmb.val		= ftdm->rdnis.type;

	redirNmb->nmbDigits.pres = PRSNT_NODEF;
	redirNmb->nmbDigits.len = len;

	memcpy(redirNmb->nmbDigits.val, ftdm->rdnis.digits, len);

	return FTDM_SUCCESS;
}


ftdm_status_t cpy_calling_name_to_sngisdn(ConEvnt *conEvnt, ftdm_channel_t *ftdmchan)
{
	uint8_t len;
	ftdm_caller_data_t *ftdm = &ftdmchan->caller_data;
	/* sngisdn_chan_data_t *sngisdn_info = ftdmchan->call_data; */
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	len = strlen(ftdm->cid_name); 
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
		memcpy(conEvnt->usrUsr.usrInfo.val, ftdm->cid_name, len);
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
			memcpy(conEvnt->display.dispInfo.val, ftdm->cid_name, len);
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



ftdm_status_t check_for_state_change(ftdm_channel_t *ftdmchan)
{

#if 0
    ftdm_log_chan_msg(ftdmchan, "Checking for pending state change\n");
#endif
    /* check to see if there are any pending state changes on the channel and give them a sec to happen*/
    ftdm_wait_for_flag_cleared(ftdmchan, FTDM_CHANNEL_STATE_CHANGE, 5000);

    /* check the flag to confirm it is clear now */
    if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
        /* the flag is still up...so we have a problem */
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "FTDM_CHANNEL_STATE_CHANGE set for over 500ms\n");

        /* move the state of the channel to RESTART to force a reset */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);

        return FTDM_FAIL;
    }
    return FTDM_SUCCESS;
}

void get_memory_info(void)
{
	U32 availmen = 0;
	SRegInfoShow(S_REG, &availmen);
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
