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
#define SNGISDN_Q931_FACILITY_IE_ID 0x1C

/* ftmod_sangoma_isdn specific enum look-up functions */

SNGISDN_ENUM_NAMES(SNGISDN_PROGIND_DESCR_NAMES, SNGISDN_PROGIND_DESCR_STRINGS)
SNGISDN_STR2ENUM(ftdm_str2ftdm_sngisdn_progind_descr, ftdm_sngisdn_progind_descr2str, ftdm_sngisdn_progind_descr_t, SNGISDN_PROGIND_DESCR_NAMES, SNGISDN_PROGIND_DESCR_INVALID)

SNGISDN_ENUM_NAMES(SNGISDN_PROGIND_LOC_NAMES, SNGISDN_PROGIND_LOC_STRINGS)
SNGISDN_STR2ENUM(ftdm_str2ftdm_sngisdn_progind_loc, ftdm_sngisdn_progind_loc2str, ftdm_sngisdn_progind_loc_t, SNGISDN_PROGIND_LOC_NAMES, SNGISDN_PROGIND_LOC_INVALID)

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

ftdm_status_t sngisdn_set_chan_avail_rate(ftdm_channel_t *chan, sngisdn_avail_t avail)
{
	if (FTDM_SPAN_IS_BRI(chan->span)) {
		ftdm_log_chan(chan, FTDM_LOG_DEBUG, "Setting availability rate to:%d\n", avail);
		chan->availability_rate = avail;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_set_span_avail_rate(ftdm_span_t *span, sngisdn_avail_t avail)
{
	if (FTDM_SPAN_IS_BRI(span)) {
		ftdm_iterator_t *chaniter = NULL;
		ftdm_iterator_t *curr = NULL;

		chaniter = ftdm_span_get_chan_iterator(span, NULL);
		for (curr = chaniter; curr; curr = ftdm_iterator_next(curr)) {
			ftdm_log_chan(((ftdm_channel_t*)ftdm_iterator_current(curr)), FTDM_LOG_DEBUG, "Setting availability rate to:%d\n", avail);
			sngisdn_set_chan_avail_rate(((ftdm_channel_t*)ftdm_iterator_current(curr)), avail);
		}
		ftdm_iterator_free(chaniter);
	}
	return FTDM_SUCCESS;
}

ftdm_status_t get_calling_num(ftdm_channel_t *ftdmchan, CgPtyNmb *cgPtyNmb)
{
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
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

ftdm_status_t get_calling_num2(ftdm_channel_t *ftdmchan, CgPtyNmb *cgPtyNmb)
{
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
	if (cgPtyNmb->eh.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}

	if (cgPtyNmb->screenInd.pres == PRSNT_NODEF) {
		ftdm_call_add_var(caller_data, "isdn.cg_pty2.screen_ind", ftdm_screening2str(cgPtyNmb->screenInd.val));
	}

	if (cgPtyNmb->presInd0.pres == PRSNT_NODEF) {
		ftdm_call_add_var(caller_data, "isdn.cg_pty2.presentation_ind", ftdm_presentation2str(cgPtyNmb->presInd0.val));
	}

	if (cgPtyNmb->nmbPlanId.pres == PRSNT_NODEF) {
		ftdm_call_add_var(caller_data, "isdn.cg_pty2.npi", ftdm_npi2str(cgPtyNmb->nmbPlanId.val));
	}
		
	if (cgPtyNmb->typeNmb1.pres == PRSNT_NODEF) {
		ftdm_call_add_var(caller_data, "isdn.cg_pty2.ton", ftdm_ton2str(cgPtyNmb->typeNmb1.val));
	}
	
	if (cgPtyNmb->nmbDigits.pres == PRSNT_NODEF) {
		char digits_string [32];
		memcpy(digits_string, (const char*)cgPtyNmb->nmbDigits.val, cgPtyNmb->nmbDigits.len);
		digits_string[cgPtyNmb->nmbDigits.len] = '\0';
		ftdm_call_add_var(caller_data, "isdn.cg_pty2.digits", digits_string);
	}
	memcpy(&caller_data->ani, &caller_data->cid_num, sizeof(caller_data->ani));
	return FTDM_SUCCESS;
}

ftdm_status_t get_called_num(ftdm_channel_t *ftdmchan, CdPtyNmb *cdPtyNmb)
{
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
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

ftdm_status_t get_redir_num(ftdm_channel_t *ftdmchan, RedirNmb *redirNmb)
{
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
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

ftdm_status_t get_calling_name_from_display(ftdm_channel_t *ftdmchan, Display *display)
{
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
	if (display->eh.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}
	if (display->dispInfo.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}
	
	ftdm_copy_string(caller_data->cid_name, (const char*)display->dispInfo.val, display->dispInfo.len+1);
	return FTDM_SUCCESS;
}

ftdm_status_t get_calling_name_from_usr_usr(ftdm_channel_t *ftdmchan, UsrUsr *usrUsr)
{
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
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

ftdm_status_t get_calling_subaddr(ftdm_channel_t *ftdmchan, CgPtySad *cgPtySad)
{
	char subaddress[100];
	
	if (cgPtySad->eh.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}
	memset(subaddress, 0, sizeof(subaddress));
	if(cgPtySad->sadInfo.len >= sizeof(subaddress)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "Calling Party Subaddress exceeds local size limit (len:%d max:%d)\n", cgPtySad->sadInfo.len, sizeof(subaddress));
		cgPtySad->sadInfo.len = sizeof(subaddress)-1;
	}
		
	memcpy(subaddress, (char*)cgPtySad->sadInfo.val, cgPtySad->sadInfo.len);
	subaddress[cgPtySad->sadInfo.len] = '\0';
	ftdm_call_add_var(&ftdmchan->caller_data, "isdn.calling_subaddr", subaddress);
	return FTDM_SUCCESS;
}

ftdm_status_t get_facility_ie(ftdm_channel_t *ftdmchan, FacilityStr *facilityStr)
{
	if (!facilityStr->eh.pres) {
		return FTDM_FAIL;
	}

	return get_facility_ie_str(ftdmchan, facilityStr->facilityStr.val, facilityStr->facilityStr.len);
}

ftdm_status_t get_facility_ie_str(ftdm_channel_t *ftdmchan, uint8_t *data, uint8_t data_len)
{
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;
	
	if (signal_data->facility_ie_decode == SNGISDN_OPT_FALSE) {
		/* size of facilityStr->facilityStr.len is a uint8_t so no need to check
		for overflow here as facilityStr->facilityStr.len will always be smaller
		than sizeof(caller_data->raw_data) */
		
		memset(caller_data->raw_data, 0, sizeof(caller_data->raw_data));
		/* Always include Facility IE identifier + len so this can be used as a sanity check by the user */
		caller_data->raw_data[0] = SNGISDN_Q931_FACILITY_IE_ID;
		caller_data->raw_data[1] = data_len;
	
		memcpy(&caller_data->raw_data[2], data, data_len);
		caller_data->raw_data_len = data_len+2;
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Raw Facility IE copied available\n");
	} else {
		/* Call libsng_isdn to process facility IE's here */
	}
	return FTDM_SUCCESS;
}

ftdm_status_t get_prog_ind_ie(ftdm_channel_t *ftdmchan, ProgInd *progInd)
{
	uint8_t val;	
	if (!progInd->eh.pres) {
		return FTDM_FAIL;
	}

	if (progInd->progDesc.pres) {
		switch (progInd->progDesc.val) {
			case IN_PD_NOTETEISDN:
				val = SNGISDN_PROGIND_DESCR_NETE_ISDN;
				break;
			case IN_PD_DSTNOTISDN:
				val = SNGISDN_PROGIND_DESCR_DEST_NISDN;
				break;
			case IN_PD_ORGNOTISDN:
				val = SNGISDN_PROGIND_DESCR_ORIG_NISDN;
				break;
			case IN_PD_CALLRET:
				val = SNGISDN_PROGIND_DESCR_RET_ISDN;
				break;
			case IN_PD_DELRESP:
				val = SNGISDN_PROGIND_DESCR_SERV_CHANGE;
				break;
			case IN_PD_IBAVAIL:
				val = SNGISDN_PROGIND_DESCR_IB_AVAIL;
				break;
			default:
				ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "Unknown Progress Indicator Description (%d)\n", progInd->progDesc.val);
				val = SNGISDN_PROGIND_DESCR_INVALID;
				break;
		}
		ftdm_call_add_var(&ftdmchan->caller_data, "isdn.prog_ind.descr", ftdm_sngisdn_progind_descr2str(val));
	}
	
	if (progInd->location.pres) {
		switch (progInd->location.val) {
			case IN_LOC_USER:
				val = SNGISDN_PROGIND_LOC_USER;
				break;
			case IN_LOC_PRIVNETLU:
				val = SNGISDN_PROGIND_LOC_PRIV_NET_LOCAL_USR;
				break;
			case IN_LOC_PUBNETLU:
				val = SNGISDN_PROGIND_LOC_PUB_NET_LOCAL_USR;
				break;
			case IN_LOC_TRANNET:
				val = SNGISDN_PROGIND_LOC_TRANSIT_NET;
				break;
			case IN_LOC_PUBNETRU:
				val = SNGISDN_PROGIND_LOC_PUB_NET_REMOTE_USR;
				break;
			case IN_LOC_PRIVNETRU:
				val = SNGISDN_PROGIND_LOC_PRIV_NET_REMOTE_USR;
				break;
			case IN_LOC_NETINTER:
				val = SNGISDN_PROGIND_LOC_NET_BEYOND_INTRW;
				break;
			default:
				ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "Unknown Progress Indicator Location (%d)", progInd->location.val);
				val = SNGISDN_PROGIND_LOC_INVALID;
				break;
		}
		ftdm_call_add_var(&ftdmchan->caller_data, "isdn.prog_ind.loc", ftdm_sngisdn_progind_loc2str(val));
	}	
	return FTDM_SUCCESS;
}


ftdm_status_t set_calling_num(ftdm_channel_t *ftdmchan, CgPtyNmb *cgPtyNmb)
{
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
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
	if (caller_data->cid_num.plan >= FTDM_NPI_INVALID) {
		cgPtyNmb->nmbPlanId.val		= FTDM_NPI_UNKNOWN;
	} else {
		cgPtyNmb->nmbPlanId.val		= caller_data->cid_num.plan;
	}

	cgPtyNmb->typeNmb1.pres		= PRSNT_NODEF;

	if (caller_data->cid_num.type >= FTDM_TON_INVALID) {
		cgPtyNmb->typeNmb1.val		= FTDM_TON_UNKNOWN;
	} else {
		cgPtyNmb->typeNmb1.val		= caller_data->cid_num.type;
	}

	cgPtyNmb->nmbDigits.pres	= PRSNT_NODEF;
	cgPtyNmb->nmbDigits.len		= len;

	memcpy(cgPtyNmb->nmbDigits.val, caller_data->cid_num.digits, len);

	return FTDM_SUCCESS;
}

ftdm_status_t set_calling_num2(ftdm_channel_t *ftdmchan, CgPtyNmb *cgPtyNmb)
{
	const char* string = NULL;
	uint8_t len,val;
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
	
	string = ftdm_call_get_var(caller_data, "isdn.cg_pty2.digits");
	if ((string == NULL) || !(*string)) {
		return FTDM_FAIL;
	}

	cgPtyNmb->eh.pres			= PRSNT_NODEF;
	
	len = strlen(string);
	cgPtyNmb->nmbDigits.len		= len;

	cgPtyNmb->nmbDigits.pres	= PRSNT_NODEF;
	memcpy(cgPtyNmb->nmbDigits.val, string, len);

	/* Screening Indicator */
	cgPtyNmb->screenInd.pres	= PRSNT_NODEF;

	val = FTDM_SCREENING_INVALID;
	string = ftdm_call_get_var(caller_data, "isdn.cg_pty2.screening_ind");
	if ((string != NULL) && (*string)) {
		val = ftdm_str2ftdm_screening(string);
	}

	/* isdn.cg_pty2.screen_ind does not exist or we could not parse its value */
	if (val == FTDM_SCREENING_INVALID) {		
		/* default to caller data screening ind */
		cgPtyNmb->screenInd.val = caller_data->screen;
	} else {
		cgPtyNmb->screenInd.val = val;
	}

	/* Presentation Indicator */
	cgPtyNmb->presInd0.pres = PRSNT_NODEF;
	
	val = FTDM_PRES_INVALID;
	string = ftdm_call_get_var(caller_data, "isdn.cg_pty2.presentation_ind");
	if ((string != NULL) && (*string)) {
		val = ftdm_str2ftdm_presentation(string);
	}

	if (val == FTDM_PRES_INVALID) {
		cgPtyNmb->presInd0.val = caller_data->pres;
	} else {
		cgPtyNmb->presInd0.val = val;
	}

	/* Numbering Plan Indicator */
	cgPtyNmb->nmbPlanId.pres	= PRSNT_NODEF;
	
	val = FTDM_NPI_INVALID;
	string = ftdm_call_get_var(caller_data, "isdn.cg_pty2.npi");
	if ((string != NULL) && (*string)) {
		val = ftdm_str2ftdm_npi(string);
	}

	if (val == FTDM_NPI_INVALID) {
		cgPtyNmb->nmbPlanId.val = caller_data->cid_num.plan;
	} else {
		cgPtyNmb->nmbPlanId.val	= val;
	}

	cgPtyNmb->typeNmb1.pres		= PRSNT_NODEF;

	/* Type of Number */
	val = FTDM_TON_INVALID;
	string = ftdm_call_get_var(caller_data, "isdn.cg_pty2.ton");
	if ((string != NULL) && (*string)) {
		val = ftdm_str2ftdm_ton(string);
	}

	if (val == FTDM_TON_INVALID) {
		cgPtyNmb->typeNmb1.val = caller_data->cid_num.type;
	} else {
		cgPtyNmb->typeNmb1.val = val;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t set_called_num(ftdm_channel_t *ftdmchan, CdPtyNmb *cdPtyNmb)
{
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
	uint8_t len = strlen(caller_data->dnis.digits);
	
	if (!len) {
		return FTDM_SUCCESS;
	}
	cdPtyNmb->eh.pres           = PRSNT_NODEF;

	cdPtyNmb->nmbPlanId.pres      = PRSNT_NODEF;
	if (caller_data->dnis.plan >= FTDM_NPI_INVALID) {
		cdPtyNmb->nmbPlanId.val       = FTDM_NPI_UNKNOWN;
	} else {
		cdPtyNmb->nmbPlanId.val       = caller_data->dnis.plan;
	}
	
	cdPtyNmb->typeNmb0.pres       = PRSNT_NODEF;
	if (caller_data->dnis.type >= FTDM_TON_INVALID) {
		cdPtyNmb->typeNmb0.val        = FTDM_TON_UNKNOWN;
	} else {
		cdPtyNmb->typeNmb0.val        = caller_data->dnis.type;
	}

	cdPtyNmb->nmbDigits.pres = PRSNT_NODEF;
	cdPtyNmb->nmbDigits.len = len;

	
	memcpy(cdPtyNmb->nmbDigits.val, caller_data->dnis.digits, len);
    return FTDM_SUCCESS;
}

ftdm_status_t set_redir_num(ftdm_channel_t *ftdmchan, RedirNmb *redirNmb)
{
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;
	uint8_t len = strlen(caller_data->rdnis.digits);
	if (!len) {
		return FTDM_SUCCESS;
	}

	redirNmb->eh.pres 	= PRSNT_NODEF;

	redirNmb->nmbPlanId.pres 	= PRSNT_NODEF;
	if (caller_data->rdnis.plan >= FTDM_NPI_INVALID) {
		redirNmb->nmbPlanId.val	= FTDM_NPI_UNKNOWN;
	} else {
		redirNmb->nmbPlanId.val = caller_data->rdnis.plan;
	}

	redirNmb->typeNmb.pres		= PRSNT_NODEF;
	if (caller_data->rdnis.type >= FTDM_TON_INVALID) {
		redirNmb->typeNmb.val		= FTDM_TON_UNKNOWN;
	} else {
		redirNmb->typeNmb.val		= caller_data->rdnis.type;
	}

	redirNmb->nmbDigits.pres = PRSNT_NODEF;
	redirNmb->nmbDigits.len = len;

	memcpy(redirNmb->nmbDigits.val, caller_data->rdnis.digits, len);

	return FTDM_SUCCESS;
}


ftdm_status_t set_calling_name(ftdm_channel_t *ftdmchan, ConEvnt *conEvnt)
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

ftdm_status_t set_calling_subaddr(ftdm_channel_t *ftdmchan, CgPtySad *cgPtySad)
{
	const char* clg_subaddr = NULL;
	clg_subaddr = ftdm_call_get_var(&ftdmchan->caller_data, "isdn.calling_subaddr");
	if ((clg_subaddr != NULL) && (*clg_subaddr)) {
		unsigned len = strlen (clg_subaddr);
		cgPtySad->eh.pres = PRSNT_NODEF;
		cgPtySad->typeSad.pres = 1;
		cgPtySad->typeSad.val = 0; /* NSAP */
		cgPtySad->oddEvenInd.pres = 1;
		cgPtySad->oddEvenInd.val = 0;

		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Sending Calling Party Subaddress:%s\n", clg_subaddr);
		cgPtySad->sadInfo.pres = 1;
		cgPtySad->sadInfo.len = len;
		memcpy(cgPtySad->sadInfo.val, clg_subaddr, len);
	}
	return FTDM_SUCCESS;
}

ftdm_status_t set_facility_ie(ftdm_channel_t *ftdmchan, FacilityStr *facilityStr)
{
	ftdm_status_t status;
	status = set_facility_ie_str(ftdmchan, facilityStr->facilityStr.val, (uint8_t*)&(facilityStr->facilityStr.len));
	if (status == FTDM_SUCCESS) {
		facilityStr->eh.pres = PRSNT_NODEF;
		facilityStr->facilityStr.pres = PRSNT_NODEF;
	}
	return status;
}

ftdm_status_t set_facility_ie_str(ftdm_channel_t *ftdmchan, uint8_t *data, uint8_t *data_len)
{
	int len;
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;

	if (caller_data->raw_data_len > 0 && caller_data->raw_data[0] == SNGISDN_Q931_FACILITY_IE_ID) {
		len = caller_data->raw_data[1];
		memcpy(data, &caller_data->raw_data[2], len);
		*data_len = len;
		return FTDM_SUCCESS;
	}	
	return FTDM_FAIL;
}

ftdm_status_t set_prog_ind_ie(ftdm_channel_t *ftdmchan, ProgInd *progInd, ftdm_sngisdn_progind_t prog_ind)
{
	const char *str = NULL;
	int descr = prog_ind.descr;
	int loc = prog_ind.loc;
	
	str = ftdm_call_get_var(&ftdmchan->caller_data, "isdn.prog_ind.descr");
	if (str && *str) {
		/* User wants to override progress indicator */
		descr = ftdm_str2ftdm_sngisdn_progind_descr(str);
	}

	if (descr == SNGISDN_PROGIND_DESCR_INVALID) {
		/* User does not want to send progress indicator */
		return FTDM_SUCCESS;
	}

	str = ftdm_call_get_var(&ftdmchan->caller_data, "isdn.prog_ind.loc");
	if (str && *str) {
		loc = ftdm_str2ftdm_sngisdn_progind_loc(str);
	}
	if (loc == SNGISDN_PROGIND_LOC_INVALID) {
		loc = SNGISDN_PROGIND_LOC_USER;
	}

	progInd->eh.pres = PRSNT_NODEF;	
	progInd->codeStand0.pres = PRSNT_NODEF;
	progInd->codeStand0.val = IN_CSTD_CCITT;
	
	progInd->progDesc.pres = PRSNT_NODEF;
	switch(descr) {
		case SNGISDN_PROGIND_DESCR_NETE_ISDN:
			progInd->progDesc.val = IN_PD_NOTETEISDN;
			break;
		case SNGISDN_PROGIND_DESCR_DEST_NISDN:
			progInd->progDesc.val = IN_PD_DSTNOTISDN;
			break;
		case SNGISDN_PROGIND_DESCR_ORIG_NISDN:
			progInd->progDesc.val = IN_PD_ORGNOTISDN;
			break;
		case SNGISDN_PROGIND_DESCR_RET_ISDN:
			progInd->progDesc.val = IN_PD_CALLRET;
			break;
		case SNGISDN_PROGIND_DESCR_SERV_CHANGE:
			/* Trillium defines do not match ITU-T Q931 Progress descriptions,
			indicate a delayed response for now */
			progInd->progDesc.val = IN_PD_DELRESP;
			break;
		case SNGISDN_PROGIND_DESCR_IB_AVAIL:
			progInd->progDesc.val = IN_PD_IBAVAIL;
			break;
		default:
			ftdm_log(FTDM_LOG_WARNING, "Invalid prog_ind description:%d\n", descr);
			progInd->progDesc.val = IN_PD_NOTETEISDN;
			break;
	}

	progInd->location.pres = PRSNT_NODEF;
	switch (loc) {
		case SNGISDN_PROGIND_LOC_USER:
			progInd->location.val = IN_LOC_USER;
			break;
		case SNGISDN_PROGIND_LOC_PRIV_NET_LOCAL_USR:
			progInd->location.val = IN_LOC_PRIVNETLU;
			break;
		case SNGISDN_PROGIND_LOC_PUB_NET_LOCAL_USR:
			progInd->location.val = IN_LOC_PUBNETLU;
			break;
		case SNGISDN_PROGIND_LOC_TRANSIT_NET:
			progInd->location.val = IN_LOC_TRANNET;
			break;
		case SNGISDN_PROGIND_LOC_PUB_NET_REMOTE_USR:
			progInd->location.val = IN_LOC_PUBNETRU;
			break;
		case SNGISDN_PROGIND_LOC_PRIV_NET_REMOTE_USR:
			progInd->location.val = IN_LOC_PRIVNETRU;
			break;
		case SNGISDN_PROGIND_LOC_NET_BEYOND_INTRW:
			progInd->location.val = IN_LOC_NETINTER;
			break;
		default:
			ftdm_log(FTDM_LOG_WARNING, "Invalid prog_ind location:%d\n", loc);
			progInd->location.val = IN_PD_NOTETEISDN;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t set_chan_id_ie(ftdm_channel_t *ftdmchan, ChanId *chanId)
{
	if (!ftdmchan) {
		return FTDM_SUCCESS;
	}
	chanId->eh.pres = PRSNT_NODEF;
	chanId->prefExc.pres = PRSNT_NODEF;
	chanId->prefExc.val = IN_PE_EXCLSVE;
	chanId->dChanInd.pres = PRSNT_NODEF;
	chanId->dChanInd.val = IN_DSI_NOTDCHAN;
	chanId->intIdentPres.pres = PRSNT_NODEF;
	chanId->intIdentPres.val = IN_IIP_IMPLICIT;

	if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI ||
		   ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP) {

		/* BRI only params */
		chanId->intType.pres = PRSNT_NODEF;
		chanId->intType.val = IN_IT_BASIC;
		chanId->infoChanSel.pres = PRSNT_NODEF;
		chanId->infoChanSel.val = ftdmchan->physical_chan_id;
	} else {
		chanId->intType.pres = PRSNT_NODEF;
		chanId->intType.val = IN_IT_OTHER;
		chanId->infoChanSel.pres = PRSNT_NODEF;
		chanId->infoChanSel.val = IN_ICS_B1CHAN;
		chanId->chanMapType.pres = PRSNT_NODEF;
		chanId->chanMapType.val = IN_CMT_BCHAN;
		chanId->nmbMap.pres = PRSNT_NODEF;
		chanId->nmbMap.val = IN_NM_CHNNMB;
		chanId->codeStand1.pres = PRSNT_NODEF;
		chanId->codeStand1.val = IN_CSTD_CCITT;
		chanId->chanNmbSlotMap.pres = PRSNT_NODEF;
		chanId->chanNmbSlotMap.len = 1;
		chanId->chanNmbSlotMap.val[0] = ftdmchan->physical_chan_id;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t set_bear_cap_ie(ftdm_channel_t *ftdmchan, BearCap *bearCap)
{
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;
	
	bearCap->eh.pres = PRSNT_NODEF;
	bearCap->infoTranCap.pres = PRSNT_NODEF;
	bearCap->infoTranCap.val = sngisdn_get_infoTranCap_from_user(ftdmchan->caller_data.bearer_capability);

	bearCap->codeStand0.pres = PRSNT_NODEF;
	bearCap->codeStand0.val = IN_CSTD_CCITT;
	bearCap->infoTranRate0.pres = PRSNT_NODEF;
	bearCap->infoTranRate0.val = IN_ITR_64KBIT;
	bearCap->tranMode.pres = PRSNT_NODEF;
	bearCap->tranMode.val = IN_TM_CIRCUIT;

	if (!FTDM_SPAN_IS_BRI(ftdmchan->span)) {
		/* Trillium stack rejests lyr1Ident on BRI, but Netbricks always sends it.
		Check with Trillium if this ever causes calls to fail in the field */

		/* PRI only params */
		bearCap->usrInfoLyr1Prot.pres = PRSNT_NODEF;
		bearCap->usrInfoLyr1Prot.val = sngisdn_get_usrInfoLyr1Prot_from_user(ftdmchan->caller_data.bearer_layer1);

		if (signal_data->switchtype == SNGISDN_SWITCH_EUROISDN &&
			bearCap->usrInfoLyr1Prot.val == IN_UIL1_G711ULAW) {

			/* We are bridging a call from T1 */
			bearCap->usrInfoLyr1Prot.val = IN_UIL1_G711ALAW;

		} else if (bearCap->usrInfoLyr1Prot.val == IN_UIL1_G711ALAW) {

			/* We are bridging a call from E1 */
			bearCap->usrInfoLyr1Prot.val = IN_UIL1_G711ULAW;
		}

		bearCap->lyr1Ident.pres = PRSNT_NODEF;
		bearCap->lyr1Ident.val = IN_L1_IDENT;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t set_restart_ind_ie(ftdm_channel_t *ftdmchan, RstInd *rstInd)
{
	rstInd->eh.pres = PRSNT_NODEF;
	rstInd->rstClass.pres = PRSNT_NODEF;
	rstInd->rstClass.val = IN_CL_INDCHAN;
	return FTDM_SUCCESS;
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

uint8_t sngisdn_get_infoTranCap_from_user(ftdm_bearer_cap_t bearer_capability)
{
	switch(bearer_capability) {
	case FTDM_BEARER_CAP_SPEECH:
		return IN_ITC_SPEECH;
	case FTDM_BEARER_CAP_64K_UNRESTRICTED:
		return IN_ITC_UNRDIG;
	case FTDM_BEARER_CAP_3_1KHZ_AUDIO:
		return IN_ITC_A31KHZ;
	case FTDM_BEARER_CAP_INVALID:
		return IN_ITC_SPEECH;
		/* Do not put a default case here, so we can see compile warnings if we have unhandled cases */
	}
	return FTDM_BEARER_CAP_SPEECH;
}

uint8_t sngisdn_get_usrInfoLyr1Prot_from_user(ftdm_user_layer1_prot_t layer1_prot)
{
	switch(layer1_prot) {
	case FTDM_USER_LAYER1_PROT_V110:
		return IN_UIL1_CCITTV110;
	case FTDM_USER_LAYER1_PROT_ULAW:
		return IN_UIL1_G711ULAW;
	case FTDM_USER_LAYER1_PROT_ALAW:
		return IN_UIL1_G711ALAW;
	case FTDM_USER_LAYER1_PROT_INVALID:
		return IN_UIL1_G711ULAW;
	/* Do not put a default case here, so we can see compile warnings if we have unhandled cases */
	}
	return IN_UIL1_G711ULAW;
}

ftdm_bearer_cap_t sngisdn_get_infoTranCap_from_stack(uint8_t bearer_capability)
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

ftdm_user_layer1_prot_t sngisdn_get_usrInfoLyr1Prot_from_stack(uint8_t layer1_prot)
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
