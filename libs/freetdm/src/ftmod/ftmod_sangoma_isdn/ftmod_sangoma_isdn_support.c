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

SNGISDN_ENUM_NAMES(SNGISDN_NETSPECFAC_TYPE_NAMES, SNGISDN_NETSPECFAC_TYPE_STRINGS)
SNGISDN_STR2ENUM(ftdm_str2ftdm_sngisdn_netspecfac_type, ftdm_sngisdn_netspecfac_type2str, ftdm_sngisdn_netspecfac_type_t, SNGISDN_NETSPECFAC_TYPE_NAMES, SNGISDN_NETSPECFAC_TYPE_INVALID)

SNGISDN_ENUM_NAMES(SNGISDN_NETSPECFAC_PLAN_NAMES, SNGISDN_NETSPECFAC_PLAN_STRINGS)
SNGISDN_STR2ENUM(ftdm_str2ftdm_sngisdn_netspecfac_plan, ftdm_sngisdn_netspecfac_plan2str, ftdm_sngisdn_netspecfac_plan_t, SNGISDN_NETSPECFAC_PLAN_NAMES, SNGISDN_NETSPECFAC_PLAN_INVALID)

SNGISDN_ENUM_NAMES(SNGISDN_NETSPECFAC_SPEC_NAMES, SNGISDN_NETSPECFAC_SPEC_STRINGS)
SNGISDN_STR2ENUM(ftdm_str2ftdm_sngisdn_netspecfac_spec, ftdm_sngisdn_netspecfac_spec2str, ftdm_sngisdn_netspecfac_spec_t, SNGISDN_NETSPECFAC_SPEC_NAMES, SNGISDN_NETSPECFAC_SPEC_INVALID)

static uint8_t _get_trillium_val(ftdm2trillium_t *vals, unsigned int num_vals, uint8_t ftdm_val, uint8_t default_val);
static uint8_t _get_ftdm_val(ftdm2trillium_t *vals, unsigned int num_vals, uint8_t trillium_val, uint8_t default_val);

#define get_trillium_val(vals, ftdm_val, default_val) _get_trillium_val(vals, ftdm_array_len(vals), ftdm_val, default_val)
#define get_ftdm_val(vals, trillium_val, default_val) _get_ftdm_val(vals, ftdm_array_len(vals), trillium_val, default_val)

ftdm_status_t get_calling_name_from_usr_usr(ftdm_channel_t *ftdmchan, UsrUsr *usrUsr);
ftdm_status_t get_calling_name_from_display(ftdm_channel_t *ftdmchan, Display *display);
ftdm_status_t get_calling_name_from_ntDisplay(ftdm_channel_t *ftdmchan, NtDisplay *display);

extern ftdm_sngisdn_data_t	g_sngisdn_data;

ftdm2trillium_t npi_codes[] = {
	{FTDM_NPI_UNKNOWN,	IN_NP_UNK},
	{FTDM_NPI_ISDN,		IN_NP_ISDN},
	{FTDM_NPI_DATA, 	IN_NP_DATA},
	{FTDM_NPI_TELEX,	IN_NP_TELEX},
	{FTDM_NPI_NATIONAL, IN_NP_NATIONAL},
	{FTDM_NPI_PRIVATE,	IN_NP_PRIVATE},
	{FTDM_NPI_RESERVED, IN_NP_EXT},
};

ftdm2trillium_t ton_codes[] = {
	{FTDM_TON_UNKNOWN,				IN_TON_UNK},
	{FTDM_TON_INTERNATIONAL,		IN_TON_INT},
	{FTDM_TON_NATIONAL,				IN_TON_NAT},
	{FTDM_TON_NETWORK_SPECIFIC, 	IN_TON_NETSPEC},
	{FTDM_TON_SUBSCRIBER_NUMBER,	IN_TON_SUB},
	{FTDM_TON_ABBREVIATED_NUMBER,	IN_TON_ABB},
	{FTDM_TON_RESERVED,				IN_TON_EXT},
};

ftdm2trillium_t nsf_spec_codes[] = {
	{SNGISDN_NETSPECFAC_SPEC_ACCUNET,		0xe6},
	{SNGISDN_NETSPECFAC_SPEC_MEGACOM,		0xe3},
	{SNGISDN_NETSPECFAC_SPEC_MEGACOM_800,	0xe2},
	{SNGISDN_NETSPECFAC_SPEC_SDDN,			0xe1},
	{SNGISDN_NETSPECFAC_SPEC_INVALID,		0x00},
};

ftdm2trillium_t nsf_type_codes[] = {
	{SNGISDN_NETSPECFAC_TYPE_USER_SPEC,						0x00},
	{SNGISDN_NETSPECFAC_TYPE_NATIONAL_NETWORK_IDENT,		0x02},
	{SNGISDN_NETSPECFAC_TYPE_INTERNATIONAL_NETWORK_IDENT,	0x03},
	{SNGISDN_NETSPECFAC_TYPE_INVALID,						0x00},
};

ftdm2trillium_t nsf_plan_codes[] = {
	{SNGISDN_NETSPECFAC_PLAN_UNKNOWN,						0x00},
	{SNGISDN_NETSPECFAC_PLAN_CARRIER_IDENT,		0x01},
	{SNGISDN_NETSPECFAC_PLAN_DATA_NETWORK_IDENT,	0x03},
	{SNGISDN_NETSPECFAC_PLAN_INVALID,						0x00},
};

static uint8_t _get_trillium_val(ftdm2trillium_t *vals, unsigned int num_vals, uint8_t ftdm_val, uint8_t default_val)
{
	int i;
	for (i = 0; i < num_vals; i++) {
		if (vals[i].ftdm_val == ftdm_val) {
			return vals[i].trillium_val;
		}
	}
	
	return default_val;
}

static uint8_t _get_ftdm_val(ftdm2trillium_t *vals, unsigned int num_vals, uint8_t trillium_val, uint8_t default_val)
{
	int i;
	for (i = 0; i < num_vals; i++) {
		if (vals[i].trillium_val == trillium_val) {
			return vals[i].ftdm_val;
		}
	}
	return default_val;
}

void clear_call_data(sngisdn_chan_data_t *sngisdn_info)
{
	uint32_t cc_id = ((sngisdn_span_data_t*)sngisdn_info->ftdmchan->span->signal_data)->cc_id;

	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_DEBUG, "Clearing call data (suId:%u suInstId:%u spInstId:%u)\n", cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);
	ftdm_mutex_lock(g_sngisdn_data.ccs[cc_id].mutex);
	g_sngisdn_data.ccs[cc_id].active_spInstIds[sngisdn_info->spInstId]=NULL;
	g_sngisdn_data.ccs[cc_id].active_suInstIds[sngisdn_info->suInstId]=NULL;
		
	sngisdn_info->suInstId = 0;
	sngisdn_info->spInstId = 0;
	sngisdn_info->globalFlg = 0;
	sngisdn_info->flags = 0;
	sngisdn_info->transfer_data.type = SNGISDN_TRANSFER_NONE;

	ftdm_mutex_unlock(g_sngisdn_data.ccs[cc_id].mutex);
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

#ifdef NETBORDER_CALL_REF
ftdm_status_t get_callref(ftdm_channel_t *ftdmchan, BCCallRef* callRef)
{
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;
	sngisdn_chan_data_t *sngisdn_info = ftdmchan->call_data;

	if (signal_data->raw_trace_q931) {
		if (callRef->eh.pres != PRSNT_NODEF || callRef->reference.pres != PRSNT_NODEF) {
			/* Netborder only supports BRI, so we only care for BRI for now */
			if (FTDM_SPAN_IS_BRI(ftdmchan->span) && !sngisdn_info->call_ref) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_WARNING, "Failed to obtain call reference\n");
			}
			return FTDM_FAIL;
		}		
		if (FTDM_SPAN_IS_BRI(ftdmchan->span)) {
			sngisdn_info->call_ref = 0x7F & callRef->reference.val;
		} else {
			sngisdn_info->call_ref = 0x7FFF & callRef->reference.val;
		}
		
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Call reference:%04x\n", sngisdn_info->call_ref);
	}
	return FTDM_SUCCESS;
}
#endif

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
		sngisdn_add_var((sngisdn_chan_data_t*)ftdmchan->call_data, "isdn.cg_pty2.screen_ind", ftdm_screening2str(cgPtyNmb->screenInd.val));
	}

	if (cgPtyNmb->presInd0.pres == PRSNT_NODEF) {
		sngisdn_add_var((sngisdn_chan_data_t*)ftdmchan->call_data, "isdn.cg_pty2.presentation_ind", ftdm_presentation2str(cgPtyNmb->presInd0.val));
	}

	if (cgPtyNmb->nmbPlanId.pres == PRSNT_NODEF) {
		sngisdn_add_var((sngisdn_chan_data_t*)ftdmchan->call_data, "isdn.cg_pty2.npi", ftdm_npi2str(cgPtyNmb->nmbPlanId.val));
	}
		
	if (cgPtyNmb->typeNmb1.pres == PRSNT_NODEF) {
		sngisdn_add_var((sngisdn_chan_data_t*)ftdmchan->call_data, "isdn.cg_pty2.ton", ftdm_ton2str(cgPtyNmb->typeNmb1.val));
	}
	
	if (cgPtyNmb->nmbDigits.pres == PRSNT_NODEF) {
		char digits_string [32];
		memcpy(digits_string, (const char*)cgPtyNmb->nmbDigits.val, cgPtyNmb->nmbDigits.len);
		digits_string[cgPtyNmb->nmbDigits.len] = '\0';
		sngisdn_add_var((sngisdn_chan_data_t*)ftdmchan->call_data, "isdn.cg_pty2.digits", digits_string);
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
		caller_data->dnis.plan = get_ftdm_val(npi_codes, cdPtyNmb->nmbPlanId.val, IN_NP_UNK);
	}

	if (cdPtyNmb->typeNmb0.pres == PRSNT_NODEF) {
		caller_data->dnis.type = get_ftdm_val(ton_codes, cdPtyNmb->typeNmb0.val, IN_TON_UNK);
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
		caller_data->rdnis.plan = get_ftdm_val(npi_codes, redirNmb->nmbPlanId.val, IN_NP_UNK);
	}

	if (redirNmb->typeNmb.pres == PRSNT_NODEF) {
		caller_data->rdnis.type = get_ftdm_val(ton_codes, redirNmb->typeNmb.val, IN_TON_UNK);
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

ftdm_status_t get_calling_name_from_ntDisplay(ftdm_channel_t *ftdmchan, NtDisplay *display)
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

ftdm_status_t get_calling_name(ftdm_channel_t *ftdmchan, ConEvnt *conEvnt)
{
	ftdm_status_t status = FTDM_FAIL;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;
	
	if (signal_data->switchtype == SNGISDN_SWITCH_DMS100) {
		status = get_calling_name_from_ntDisplay(ftdmchan, &conEvnt->ntDisplay[0]);
	} else {
		status = get_calling_name_from_display(ftdmchan, &conEvnt->display);

	}
	if (status != FTDM_SUCCESS) {
		status = get_calling_name_from_usr_usr(ftdmchan, &conEvnt->usrUsr);
	}
	return status;
}


ftdm_status_t get_calling_subaddr(ftdm_channel_t *ftdmchan, CgPtySad *cgPtySad)
{
	char subaddress[100];
	
	if (cgPtySad->eh.pres != PRSNT_NODEF) {
		return FTDM_FAIL;
	}
	memset(subaddress, 0, sizeof(subaddress));
	if(cgPtySad->sadInfo.len >= sizeof(subaddress)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "Calling Party Subaddress exceeds local size limit (len:%d max:%"FTDM_SIZE_FMT")\n", cgPtySad->sadInfo.len, sizeof(subaddress));
		cgPtySad->sadInfo.len = sizeof(subaddress)-1;
	}
		
	memcpy(subaddress, (char*)cgPtySad->sadInfo.val, cgPtySad->sadInfo.len);
	subaddress[cgPtySad->sadInfo.len] = '\0';
	sngisdn_add_var((sngisdn_chan_data_t*)ftdmchan->call_data, "isdn.calling_subaddr", subaddress);
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
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;
	
	if (signal_data->facility_ie_decode == SNGISDN_OPT_FALSE) {
		/* Max size of Facility IE is 255 */
		uint8_t my_data [255];
		
		/* Always include Facility IE identifier + len so this can be used as a sanity check by the user */
		my_data[0] = SNGISDN_Q931_FACILITY_IE_ID;
		my_data[1] = data_len;
		memcpy(&my_data[2], data, data_len);

		sngisdn_add_raw_data((sngisdn_chan_data_t*)ftdmchan->call_data, my_data, data_len+2);
		
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
		/* TODO: use get_ftdm_val function and table here */
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
		sngisdn_add_var((sngisdn_chan_data_t*)ftdmchan->call_data, "isdn.prog_ind.descr", ftdm_sngisdn_progind_descr2str(val));
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
		sngisdn_add_var((sngisdn_chan_data_t*)ftdmchan->call_data, "isdn.prog_ind.loc", ftdm_sngisdn_progind_loc2str(val));
	}	
	return FTDM_SUCCESS;
}


ftdm_status_t get_network_specific_fac(ftdm_channel_t *ftdmchan, NetFac *netFac)
{
	if (!netFac->eh.pres) {
		return FTDM_FAIL;
	}

	if (netFac->netFacSpec.pres == PRSNT_NODEF) {
		char digits_string [32];
		memcpy(digits_string, (const char*)netFac->netFacSpec.val, netFac->netFacSpec.len);
		digits_string[netFac->netFacSpec.len] = '\0';
		sngisdn_add_var((sngisdn_chan_data_t*)ftdmchan->call_data, "isdn.netFac.spec", digits_string);
	}

	if (netFac->typeNetId.pres == PRSNT_NODEF) {
		sngisdn_add_var((sngisdn_chan_data_t*)ftdmchan->call_data, "isdn.netFac.type", ftdm_sngisdn_netspecfac_type2str(get_ftdm_val(nsf_type_codes, netFac->typeNetId.val, 0x00)));
	}

	if (netFac->netIdPlan.pres == PRSNT_NODEF) {
		sngisdn_add_var((sngisdn_chan_data_t*)ftdmchan->call_data, "isdn.netFac.plan", ftdm_sngisdn_netspecfac_type2str(get_ftdm_val(nsf_plan_codes, netFac->netIdPlan.val, 0x00)));
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

	if (!strncasecmp(caller_data->cid_num.digits, "0000000000", strlen("0000000000"))) {
		return FTDM_SUCCESS;
	}

	cgPtyNmb->eh.pres			= PRSNT_NODEF;

	cgPtyNmb->screenInd.pres	= PRSNT_NODEF;
	cgPtyNmb->screenInd.val		= caller_data->screen;

	cgPtyNmb->presInd0.pres     = PRSNT_NODEF;
	cgPtyNmb->presInd0.val      = caller_data->pres;
	
	cgPtyNmb->nmbPlanId.pres	= PRSNT_NODEF;
	cgPtyNmb->nmbPlanId.val = get_trillium_val(npi_codes, caller_data->cid_num.plan, IN_NP_UNK);

	cgPtyNmb->typeNmb1.pres		= PRSNT_NODEF;

	cgPtyNmb->typeNmb1.val = get_trillium_val(ton_codes, caller_data->cid_num.type, IN_TON_UNK);

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
	
	string = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.cg_pty2.digits");
	if (ftdm_strlen_zero(string)) {
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
	string = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.cg_pty2.screening_ind");
	if (!ftdm_strlen_zero(string)) {
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
	string = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.cg_pty2.presentation_ind");
	if (!ftdm_strlen_zero(string)) {
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
	string = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.cg_pty2.npi");
	if (!ftdm_strlen_zero(string)) {
		val = ftdm_str2ftdm_npi(string);
	}

	if (val == FTDM_NPI_INVALID) {
		cgPtyNmb->nmbPlanId.val = caller_data->cid_num.plan;
	} else {
		cgPtyNmb->nmbPlanId.val = get_trillium_val(npi_codes, val, IN_NP_UNK);
	}

	cgPtyNmb->typeNmb1.pres		= PRSNT_NODEF;

	/* Type of Number */
	val = FTDM_TON_INVALID;
	string = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.cg_pty2.ton");
	if (!ftdm_strlen_zero(string)) {
		val = ftdm_str2ftdm_ton(string);
	}

	if (val == FTDM_TON_INVALID) {
		cgPtyNmb->typeNmb1.val = caller_data->cid_num.type;
	} else {
		cgPtyNmb->typeNmb1.val = get_trillium_val(ton_codes, val, IN_TON_UNK);
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
	
	cdPtyNmb->eh.pres			= PRSNT_NODEF;

	cdPtyNmb->nmbPlanId.pres	= PRSNT_NODEF;
	cdPtyNmb->nmbPlanId.val		= get_trillium_val(npi_codes, caller_data->dnis.plan, IN_NP_UNK);
	
	cdPtyNmb->typeNmb0.pres		= PRSNT_NODEF;
	cdPtyNmb->typeNmb0.val		= get_trillium_val(ton_codes, caller_data->dnis.type, IN_TON_UNK);

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
	redirNmb->nmbPlanId.val		= get_trillium_val(npi_codes, caller_data->rdnis.plan, IN_NP_UNK);

	redirNmb->typeNmb.pres		= PRSNT_NODEF;
	redirNmb->typeNmb.val		= get_trillium_val(ton_codes, caller_data->rdnis.type, IN_TON_UNK);

	redirNmb->nmbDigits.pres = PRSNT_NODEF;
	redirNmb->nmbDigits.len = len;

	memcpy(redirNmb->nmbDigits.val, caller_data->rdnis.digits, len);

	return FTDM_SUCCESS;
}


ftdm_status_t set_calling_name(ftdm_channel_t *ftdmchan, ConEvnt *conEvnt)
{
	uint8_t len;
	const char *string = NULL;
	ftdm_caller_data_t *caller_data = &ftdmchan->caller_data;	
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;
	ftdm_bool_t force_send_cid_name = FTDM_FALSE;
	
	len = strlen(caller_data->cid_name);
	if (!len) {
		return FTDM_SUCCESS;
	}

	string = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.send_cid_name");
	if (!ftdm_strlen_zero(string)) {
		if (!strcasecmp(string, "no")) {
			return FTDM_SUCCESS;
		} else if (!strcasecmp(string, "yes")) {
			force_send_cid_name = FTDM_TRUE;
		}
	}

	if (force_send_cid_name == FTDM_FALSE && signal_data->send_cid_name == SNGISDN_OPT_FALSE) {
		return FTDM_SUCCESS;
	}

	switch(signal_data->cid_name_method) {
		case SNGISDN_CID_NAME_FACILITY_IE:
#ifdef SNGISDN_SUPPORT_CALLING_NAME_IN_FACILITY
			/* Note: The Facility IE will be overwritten if user chose to transmit a Raw Facility IE */
			sng_isdn_encode_facility_caller_name(caller_data->cid_name, conEvnt->facilityStr.facilityStr.val, &conEvnt->facilityStr.facilityStr.len);
			conEvnt->facilityStr.eh.pres = PRSNT_NODEF;
			conEvnt->facilityStr.facilityStr.pres = PRSNT_NODEF;
#endif
			break;
		case SNGISDN_CID_NAME_USR_USR_IE:
			conEvnt->usrUsr.eh.pres = PRSNT_NODEF;
			conEvnt->usrUsr.protocolDisc.pres = PRSNT_NODEF;
			conEvnt->usrUsr.protocolDisc.val = PD_IA5; /* IA5 chars */
			conEvnt->usrUsr.usrInfo.pres = PRSNT_NODEF;
			conEvnt->usrUsr.usrInfo.len = len;
			/* in sangoma_brid we used to send usr-usr info as <cid_name>!<calling_number>,
			change to previous style if current one does not work */
			memcpy(conEvnt->usrUsr.usrInfo.val, caller_data->cid_name, len);
			break;
		case SNGISDN_CID_NAME_DISPLAY_IE:
			if (signal_data->switchtype == SNGISDN_SWITCH_DMS100) {
				conEvnt->ntDisplay[0].eh.pres = PRSNT_NODEF;
				conEvnt->ntDisplay[0].dispTypeNt.pres = PRSNT_NODEF;
				conEvnt->ntDisplay[0].dispTypeNt.val = 0x01; /* Calling Party Name */
				conEvnt->ntDisplay[0].assocInfo.pres = PRSNT_NODEF;
				conEvnt->ntDisplay[0].assocInfo.val  = 0x03; /* Included */
				conEvnt->ntDisplay[0].eh.pres = PRSNT_NODEF;
				conEvnt->ntDisplay[0].eh.pres = PRSNT_NODEF;
				conEvnt->ntDisplay[0].dispInfo.pres = PRSNT_NODEF;
				conEvnt->ntDisplay[0].dispInfo.len = len;
				memcpy(conEvnt->ntDisplay[0].dispInfo.val, caller_data->cid_name, len);
			} else {
				conEvnt->display.eh.pres = PRSNT_NODEF;
				conEvnt->display.dispInfo.pres = PRSNT_NODEF;
				conEvnt->display.dispInfo.len = len;
				memcpy(conEvnt->display.dispInfo.val, caller_data->cid_name, len);
			}
			break;
		default:
			break;
	}

	return FTDM_SUCCESS;
}

ftdm_status_t set_calling_subaddr(ftdm_channel_t *ftdmchan, CgPtySad *cgPtySad)
{
	const char* clg_subaddr = NULL;
	clg_subaddr = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.calling_subaddr");
	if (!ftdm_strlen_zero(clg_subaddr)) {
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

ftdm_status_t set_called_subaddr(ftdm_channel_t *ftdmchan, CdPtySad *cdPtySad)
{
	const char* cld_subaddr = NULL;
	cld_subaddr = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.called_subaddr");
	if (!ftdm_strlen_zero(cld_subaddr)) {
		unsigned len = strlen (cld_subaddr);
		cdPtySad->eh.pres = PRSNT_NODEF;
		cdPtySad->typeSad.pres = 1;
		cdPtySad->typeSad.val = 0; /* NSAP */
		cdPtySad->oddEvenInd.pres = 1;
		cdPtySad->oddEvenInd.val = 0;

		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Sending Called Party Subaddress:%s\n", cld_subaddr);
		cdPtySad->sadInfo.pres = 1;
		cdPtySad->sadInfo.len = len;
		memcpy(cdPtySad->sadInfo.val, cld_subaddr, len);
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
	ftdm_size_t len;
	uint8_t *mydata;
	void *vdata;

	if (ftdm_usrmsg_get_raw_data(ftdmchan->usrmsg, &vdata, &len) == FTDM_SUCCESS) {
		mydata = vdata;
		if (len > 2 && mydata[0] == SNGISDN_Q931_FACILITY_IE_ID) {
			len = mydata[1];
			memcpy(data, &mydata[2], len);
			*data_len = len;
			return FTDM_SUCCESS;
		}
	}
	return FTDM_FAIL;
}

ftdm_status_t set_prog_ind_ie(ftdm_channel_t *ftdmchan, ProgInd *progInd, ftdm_sngisdn_progind_t prog_ind)
{
	const char *str = NULL;
	int descr = prog_ind.descr;
	int loc = prog_ind.loc;
	
	str = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.prog_ind.descr");
	if (!ftdm_strlen_zero(str)) {
		/* User wants to override progress indicator */
		descr = ftdm_str2ftdm_sngisdn_progind_descr(str);
	}

	if (descr == SNGISDN_PROGIND_DESCR_INVALID) {
		/* User does not want to send progress indicator */
		return FTDM_SUCCESS;
	}

	str = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.prog_ind.loc");
	if (!ftdm_strlen_zero(str)) {
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
			progInd->location.val = IN_LOC_USER;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t set_network_specific_fac(ftdm_channel_t *ftdmchan, NetFac *netFac)
{
	const char *str = NULL;
	
	str = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.netFac.spec");
	if (ftdm_strlen_zero(str)) {
		/* Network-specific facility specification is mandatory, cannot send IE
			without it */
		return FTDM_SUCCESS;
	} else {
		ftdm_sngisdn_netspecfac_spec_t spec = ftdm_str2ftdm_sngisdn_netspecfac_spec(str);

		netFac->eh.pres = PRSNT_NODEF;
		netFac->netFacSpec.pres = PRSNT_NODEF;

		if (spec == SNGISDN_NETSPECFAC_SPEC_INVALID) {
			int byte = 0;
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Non-standard NSF specified:%s\n", str);

			if (sscanf(str, "%x", &byte) == 1) {
				netFac->netFacSpec.val[0] = byte & 0xFF;
			}

			netFac->netFacSpec.len = 1;
		} else {
			/* User is using one of the pre-specified NSF's */
			netFac->netFacSpec.val[0] = get_trillium_val(nsf_spec_codes, spec, 0x00);
			netFac->netFacSpec.len = 1;
		}
	}

	netFac->lenNetId.pres = PRSNT_NODEF;
	netFac->lenNetId.val = 0;

	str = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.netFac.type");
	if (!ftdm_strlen_zero(str)) {
		netFac->typeNetId.pres = PRSNT_NODEF;
		netFac->typeNetId.val = ftdm_str2ftdm_sngisdn_netspecfac_type(str);
	}

	str = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.netFac.plan"); 
	if (!ftdm_strlen_zero(str)) {
		netFac->netIdPlan.pres = PRSNT_NODEF;
		netFac->netIdPlan.val = ftdm_str2ftdm_sngisdn_netspecfac_plan(str);
	}

	if (netFac->netIdPlan.pres == PRSNT_NODEF || netFac->typeNetId.pres == PRSNT_NODEF) {
		netFac->lenNetId.val++;
	}

	str = ftdm_usrmsg_get_var(ftdmchan->usrmsg, "isdn.netFac.ident");
	if (!ftdm_strlen_zero(str)) {
		netFac->lenNetId.val++;

		netFac->netId.pres = PRSNT_NODEF;
		memcpy(netFac->netId.val, str, strlen(str));
	}

	return FTDM_SUCCESS;	
}

ftdm_status_t set_user_to_user_ie(ftdm_channel_t *ftdmchan, UsrUsr *usrUsr)
{
	sngisdn_chan_data_t *sngisdn_info = ftdmchan->call_data;

	if (sngisdn_info->transfer_data.type == SNGISDN_TRANSFER_ATT_COURTESY_VRU_DATA) {
		usrUsr->eh.pres = PRSNT_NODEF;

		usrUsr->protocolDisc.pres = PRSNT_NODEF;
		usrUsr->protocolDisc.val = 0x08;
		usrUsr->usrInfo.pres = PRSNT_NODEF;
		usrUsr->usrInfo.len = strlen(sngisdn_info->transfer_data.tdata.att_courtesy_vru.data);
		memcpy(usrUsr->usrInfo.val, sngisdn_info->transfer_data.tdata.att_courtesy_vru.data, usrUsr->usrInfo.len);
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Sending AT&T Transfer data len:%d\n", usrUsr->usrInfo.len);

		return FTDM_SUCCESS;
	}

	return FTDM_SUCCESS;
}

ftdm_status_t set_cause_ie(ftdm_channel_t *ftdmchan, CauseDgn *causeDgn)
{

	causeDgn->eh.pres = PRSNT_NODEF;
	causeDgn->location.pres = PRSNT_NODEF;
	causeDgn->location.val = IN_LOC_PRIVNETLU;
	causeDgn->codeStand3.pres = PRSNT_NODEF;
	causeDgn->codeStand3.val = IN_CSTD_CCITT;
	causeDgn->causeVal.pres = PRSNT_NODEF;
	causeDgn->causeVal.val = ftdmchan->caller_data.hangup_cause;
	causeDgn->recommend.pres = NOTPRSNT;
	causeDgn->dgnVal.pres = NOTPRSNT;
	return FTDM_SUCCESS;
}

ftdm_status_t set_chan_id_ie(ftdm_channel_t *ftdmchan, ChanId *chanId)
{
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*)ftdmchan->call_data;
	if (!ftdmchan) {
		return FTDM_SUCCESS;
	}
	
	ftdm_set_flag(sngisdn_info, FLAG_SENT_CHAN_ID);

	chanId->eh.pres = PRSNT_NODEF;
	chanId->prefExc.pres = PRSNT_NODEF;
	chanId->prefExc.val = IN_PE_EXCLSVE;
	chanId->dChanInd.pres = PRSNT_NODEF;
	chanId->dChanInd.val = IN_DSI_NOTDCHAN;
	chanId->intIdentPres.pres = PRSNT_NODEF;
	chanId->intIdentPres.val = IN_IIP_IMPLICIT;

	if (FTDM_SPAN_IS_BRI(ftdmchan->span)) {

		/* BRI only params */
		chanId->intType.pres = PRSNT_NODEF;
		chanId->intType.val = IN_IT_BASIC;
		chanId->infoChanSel.pres = PRSNT_NODEF;
		chanId->infoChanSel.val = ftdmchan->physical_chan_id;
	} else {
		if (signal_data->nfas.trunk) {
			chanId->intIdentPres.val = IN_IIP_EXPLICIT;
			chanId->intIdent.pres = PRSNT_NODEF;
			chanId->intIdent.val = signal_data->nfas.interface_id;
		}

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

	bearCap->usrInfoLyr1Prot.pres = PRSNT_NODEF;
	bearCap->usrInfoLyr1Prot.val = sngisdn_get_usrInfoLyr1Prot_from_user(ftdmchan->caller_data.bearer_layer1);

	switch (signal_data->switchtype) {
		case SNGISDN_SWITCH_NI2:
		case SNGISDN_SWITCH_4ESS:
		case SNGISDN_SWITCH_5ESS:
		case SNGISDN_SWITCH_DMS100:
		case SNGISDN_SWITCH_INSNET:
			if (bearCap->usrInfoLyr1Prot.val == IN_UIL1_G711ALAW) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Overriding bearer cap to u-law\n");
				bearCap->usrInfoLyr1Prot.val = IN_UIL1_G711ULAW;
			}
			break;
		case SNGISDN_SWITCH_EUROISDN:
		case SNGISDN_SWITCH_QSIG:
			if (bearCap->usrInfoLyr1Prot.val == IN_UIL1_G711ULAW) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Overriding bearer cap to a-law\n");
				bearCap->usrInfoLyr1Prot.val = IN_UIL1_G711ALAW;
			}
			break;
	}

	bearCap->lyr1Ident.pres = PRSNT_NODEF;
	bearCap->lyr1Ident.val = IN_L1_IDENT;
	
	return FTDM_SUCCESS;
}

ftdm_status_t set_restart_ind_ie(ftdm_channel_t *ftdmchan, RstInd *rstInd)
{
	rstInd->eh.pres = PRSNT_NODEF;
	rstInd->rstClass.pres = PRSNT_NODEF;
	rstInd->rstClass.val = IN_CL_INDCHAN;
	return FTDM_SUCCESS;
}

ftdm_status_t set_not_ind_ie(ftdm_channel_t *ftdmchan, NotInd *notInd)
{
	notInd->eh.pres = PRSNT_NODEF;
	notInd->notDesc.pres = PRSNT_NODEF;
	notInd->notDesc.val = 0x71; /* Call information event */
	return FTDM_SUCCESS;
}

void sngisdn_t3_timeout(void *p_sngisdn_info)
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


void sngisdn_delayed_dl_req(void *p_signal_data)
{
	ftdm_signaling_status_t sigstatus = FTDM_SIG_STATE_DOWN;	
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t *)p_signal_data;
	ftdm_span_t *span = signal_data->ftdm_span;
	
	if (!signal_data->dl_request_pending) {
		return;
	}
	
	ftdm_span_get_sig_status(span, &sigstatus);
	if (sigstatus == FTDM_SIG_STATE_UP) {
		signal_data->dl_request_pending = 0;
		return;
	}

	sngisdn_snd_dl_req(span->channels[1]);
	ftdm_sched_timer(signal_data->sched, "delayed_dl_req", 4000, sngisdn_delayed_dl_req, (void*) signal_data, NULL);

	return;
}

void sngisdn_restart_timeout(void *p_signal_data)
{
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t *)p_signal_data;
	ftdm_span_t *span = signal_data->ftdm_span;
	ftdm_iterator_t *chaniter = NULL;
	ftdm_iterator_t *curr = NULL;

	ftdm_log(FTDM_LOG_DEBUG, "s%s:Did not receive a RESTART from remote switch in %d ms - restarting\n", span->name, signal_data->restart_timeout);

	chaniter = ftdm_span_get_chan_iterator(span, NULL);
	for (curr = chaniter; curr; curr = ftdm_iterator_next(curr)) {
		ftdm_channel_t *ftdmchan = (ftdm_channel_t*)ftdm_iterator_current(curr);
		if (FTDM_IS_VOICE_CHANNEL(ftdmchan)) {
			ftdm_mutex_lock(ftdmchan->mutex);
			if (ftdmchan->state == FTDM_CHANNEL_STATE_DOWN) {
				ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_RESET);
			}
			ftdm_mutex_unlock(ftdmchan->mutex);
		}
	}
	return;
}

void sngisdn_delayed_setup(void *p_sngisdn_info)
{
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*)p_sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;

	ftdm_mutex_lock(ftdmchan->mutex);
	sngisdn_snd_setup(ftdmchan);
	ftdm_mutex_unlock(ftdmchan->mutex);
	return;
}

void sngisdn_delayed_release_nfas(void *p_sngisdn_info)
{
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*)p_sngisdn_info;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	ftdm_mutex_lock(ftdmchan->mutex);
	
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Sending delayed RELEASE (suId:%d suInstId:%u spInstId:%u)\n",
					signal_data->cc_id, sngisdn_info->spInstId, sngisdn_info->suInstId);

	sngisdn_snd_release(ftdmchan, 0);

	ftdm_mutex_unlock(ftdmchan->mutex);
	return;
}

void sngisdn_delayed_release(void *p_sngisdn_info)
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

void sngisdn_delayed_connect(void *p_sngisdn_info)
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

void sngisdn_delayed_disconnect(void *p_sngisdn_info)
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

void sngisdn_facility_timeout(void *p_sngisdn_info)
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

void sngisdn_get_memory_info(void)
{
#ifdef WIN32
	/* SRegInfoShow is not formally supported by Trillium with Windows */
	ftdm_log(FTDM_LOG_WARNING, "SRegInfoShow not supported on Windows\n");
#else	
	/* SRegInfoShow is not formally supported by Trillium in Linux either, but
	 * it seems like its working fine so far */
	U32 availmen = 0;
	SRegInfoShow(S_REG, &availmen);
#endif	
	return;
}


uint8_t sngisdn_get_infoTranCap_from_user(ftdm_bearer_cap_t bearer_capability)
{
	switch(bearer_capability) {
	case FTDM_BEARER_CAP_SPEECH:
		return IN_ITC_SPEECH;
	case FTDM_BEARER_CAP_UNRESTRICTED:
		return IN_ITC_UNRDIG;
	case FTDM_BEARER_CAP_3_1KHZ_AUDIO:
		return IN_ITC_A31KHZ;
	case FTDM_BEARER_CAP_INVALID:
		return IN_ITC_SPEECH;
	default:
		return IN_ITC_SPEECH;
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
	default:
		return IN_UIL1_G711ULAW;
	}
	return IN_UIL1_G711ULAW;
}

ftdm_bearer_cap_t sngisdn_get_infoTranCap_from_stack(uint8_t bearer_capability)
{
	switch(bearer_capability) {
	case IN_ITC_SPEECH:
		return FTDM_BEARER_CAP_SPEECH;
	case IN_ITC_UNRDIG:
		return FTDM_BEARER_CAP_UNRESTRICTED;
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

ftdm_status_t sngisdn_show_l1_stats(ftdm_stream_handle_t *stream, ftdm_span_t *span)
{
	L1Mngmt sts;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;

	memset(&sts, 0, sizeof(sts));
	sng_isdn_phy_stats(sngisdn_dchan(signal_data)->link_id , &sts);

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
	return FTDM_SUCCESS;
}


ftdm_status_t sngisdn_show_span(ftdm_stream_handle_t *stream, ftdm_span_t *span)
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
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_show_spans(ftdm_stream_handle_t *stream)
{
	int i;	
	for(i=1;i<=MAX_L1_LINKS;i++) {		
		if (g_sngisdn_data.spans[i]) {
			sngisdn_show_span(stream, g_sngisdn_data.spans[i]->ftdm_span);
		}
	}
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_add_var(sngisdn_chan_data_t *sngisdn_info, const char* var, const char* val)
{
	char *t_name = 0, *t_val = 0;
	if (!var || !val) {
		return FTDM_FAIL;
	}
	if (!sngisdn_info->variables) {
		/* initialize on first use */
		sngisdn_info->variables = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
		ftdm_assert_return(sngisdn_info->variables, FTDM_FAIL, "Failed to create hash table\n");
	}
	t_name = ftdm_strdup(var);
	t_val = ftdm_strdup(val);
	hashtable_insert(sngisdn_info->variables, t_name, t_val, HASHTABLE_FLAG_FREE_KEY | HASHTABLE_FLAG_FREE_VALUE);
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_add_raw_data(sngisdn_chan_data_t *sngisdn_info, uint8_t* data, ftdm_size_t data_len)
{
	ftdm_assert_return(!sngisdn_info->raw_data, FTDM_FAIL, "Overwriting existing raw data\n");
	
	sngisdn_info->raw_data = ftdm_calloc(1, data_len);
	ftdm_assert_return(sngisdn_info->raw_data, FTDM_FAIL, "Failed to allocate raw data\n");

	memcpy(sngisdn_info->raw_data, data, data_len);
	sngisdn_info->raw_data_len = data_len;
	return FTDM_SUCCESS;
}

void sngisdn_send_signal(sngisdn_chan_data_t *sngisdn_info, ftdm_signal_event_t event_id)
{
	ftdm_sigmsg_t sigev;
	ftdm_channel_t *ftdmchan = sngisdn_info->ftdmchan;

	memset(&sigev, 0, sizeof(sigev));

	sigev.chan_id = ftdmchan->chan_id;
	sigev.span_id = ftdmchan->span_id;
	sigev.channel = ftdmchan;
	sigev.event_id = event_id;

	if (sngisdn_info->variables) {
		/*
		* variables now belongs to the ftdm core, and
		* will be cleared after sigev is processed by user. Set
		* local pointer to NULL so we do not attempt to
		* destroy it */
		sigev.variables = sngisdn_info->variables;
		sngisdn_info->variables = NULL;
	}

	if (sngisdn_info->raw_data) {
		/*
		* raw_data now belongs to the ftdm core, and
		* will be cleared after sigev is processed by user. Set
		* local pointer to NULL so we do not attempt to
		* destroy it */
		
		sigev.raw.data = sngisdn_info->raw_data;
		sigev.raw.len = sngisdn_info->raw_data_len;

		sngisdn_info->raw_data = NULL;
		sngisdn_info->raw_data_len = 0;
	}
	if (event_id == FTDM_SIGEVENT_TRANSFER_COMPLETED) {
		sigev.ev_data.transfer_completed.response = sngisdn_info->transfer_data.response;
	}
	ftdm_span_send_signal(ftdmchan->span, &sigev);
}

sngisdn_span_data_t *sngisdn_dchan(sngisdn_span_data_t *signal_data)
{
	if (!signal_data) {
		return NULL;
	}

	if (!signal_data->nfas.trunk) {
		return signal_data;
	}
	return signal_data->nfas.trunk->dchan;
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

/******************************************************************************/
