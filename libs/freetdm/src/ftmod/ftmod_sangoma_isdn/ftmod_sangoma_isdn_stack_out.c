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

void sngisdn_snd_setup(ftdm_channel_t *ftdmchan)
{
	ConEvnt conEvnt;
	sngisdn_chan_data_t *sngisdn_info = ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;
	ftdm_sngisdn_progind_t prog_ind = {SNGISDN_PROGIND_LOC_USER, SNGISDN_PROGIND_DESCR_INVALID};	

	ftdm_assert((!sngisdn_info->suInstId && !sngisdn_info->spInstId), "Trying to call out, but call data was not cleared\n");
	
	sngisdn_info->suInstId = get_unique_suInstId(signal_data->cc_id);
	sngisdn_info->spInstId = 0;

	ftdm_mutex_lock(g_sngisdn_data.ccs[signal_data->cc_id].mutex);
	g_sngisdn_data.ccs[signal_data->cc_id].active_suInstIds[sngisdn_info->suInstId] = sngisdn_info;
	ftdm_mutex_unlock(g_sngisdn_data.ccs[signal_data->cc_id].mutex);

	memset(&conEvnt, 0, sizeof(conEvnt));
	if (signal_data->switchtype == SNGISDN_SWITCH_EUROISDN || signal_data->force_sending_complete == SNGISDN_OPT_TRUE) {
		conEvnt.sndCmplt.eh.pres = PRSNT_NODEF;
	}
	
	if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP &&
		signal_data->signalling == SNGISDN_SIGNALING_NET) {
		sngisdn_info->ces = CES_MNGMNT;
	}
	ftdm_log_chan(sngisdn_info->ftdmchan, FTDM_LOG_INFO, "Outgoing call: Called No:[%s] Calling No:[%s]\n", ftdmchan->caller_data.dnis.digits, ftdmchan->caller_data.cid_num.digits);

	set_chan_id_ie(ftdmchan, &conEvnt.chanId);	
	set_bear_cap_ie(ftdmchan, &conEvnt.bearCap[0]);
	set_called_num(ftdmchan, &conEvnt.cdPtyNmb);
	set_calling_num(ftdmchan, &conEvnt.cgPtyNmb);
	set_calling_num2(ftdmchan, &conEvnt.cgPtyNmb2);
	set_calling_subaddr(ftdmchan, &conEvnt.cgPtySad);
	set_called_subaddr(ftdmchan, &conEvnt.cdPtySad);
	set_redir_num(ftdmchan, &conEvnt.redirNmb);
	set_calling_name(ftdmchan, &conEvnt);
	set_network_specific_fac(ftdmchan, &conEvnt.netFac[0]);

	/* set_facility_ie will overwrite Calling Name for NI-2 if user specifies custom Facility IE */
	set_facility_ie(ftdmchan, &conEvnt.facilityStr);
	set_prog_ind_ie(ftdmchan, &conEvnt.progInd, prog_ind);

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending SETUP (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces);

	if (sng_isdn_con_request(signal_data->cc_id, sngisdn_info->suInstId, &conEvnt, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "stack refused SETUP request\n");
	}

	return;
}

/* Unsed only for overlap receive */
void sngisdn_snd_setup_ack(ftdm_channel_t *ftdmchan)
{
	CnStEvnt cnStEvnt;
	
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	if (!sngisdn_info->suInstId || !sngisdn_info->spInstId) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Sending SETUP ACK , but no call data, aborting (suId:%d suInstId:%u spInstId:%u)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);
		sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
		return;
	}
	
	memset(&cnStEvnt, 0, sizeof(cnStEvnt));	



	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending SETUP ACK (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces);

	if(sng_isdn_con_status(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, &cnStEvnt, MI_SETUPACK, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused SETUP ACK request\n");
	}
	return;
}


/* Used only for BRI PTMP - This function is used when the NT side makes a call out,
	and one or multiple TE's reply, then NT assigns the call by sending a con_complete*/
void sngisdn_snd_con_complete(ftdm_channel_t *ftdmchan)
{
	CnStEvnt cnStEvnt;
	
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	if (!sngisdn_info->suInstId || !sngisdn_info->spInstId) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Sending CONNECT COMPL , but no call data, aborting (suId:%d suInstId:%u spInstId:%u)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);
		sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
		return;
	}
	
	memset(&cnStEvnt, 0, sizeof(cnStEvnt));

	/* Indicate channel ID only in first response */
	if (!ftdm_test_flag(sngisdn_info, FLAG_SENT_CHAN_ID)) {
		set_chan_id_ie(ftdmchan, &cnStEvnt.chanId);
	}

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending CONNECT COMPL (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces);

	if(sng_isdn_con_comp(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, &cnStEvnt, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused CONNECT ACK request\n");
	}
	return;
}


void sngisdn_snd_proceed(ftdm_channel_t *ftdmchan, ftdm_sngisdn_progind_t prog_ind)
{
	CnStEvnt cnStEvnt;
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	if (sngisdn_test_flag(sngisdn_info, FLAG_SENT_PROCEED)) {
		return;
	}
	sngisdn_set_flag(sngisdn_info, FLAG_SENT_PROCEED);

	if (!sngisdn_info->suInstId || !sngisdn_info->spInstId) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Sending PROGRESS, but no call data, aborting (suId:%d suInstId:%u spInstId:%u)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);
		sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
		return;
	}

	memset(&cnStEvnt, 0, sizeof(cnStEvnt));

	/* Indicate channel ID only in first response */
	if (!ftdm_test_flag(sngisdn_info, FLAG_SENT_CHAN_ID)) {
		set_chan_id_ie(ftdmchan, &cnStEvnt.chanId);
	}
	set_prog_ind_ie(ftdmchan, &cnStEvnt.progInd, prog_ind);
	set_facility_ie(ftdmchan, &cnStEvnt.facilityStr);

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending PROCEED (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces);

	if(sng_isdn_con_status(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, &cnStEvnt, MI_CALLPROC, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused PROCEED request\n");
	}
	return;
}

void sngisdn_snd_progress(ftdm_channel_t *ftdmchan, ftdm_sngisdn_progind_t prog_ind)
{
	CnStEvnt cnStEvnt;
	
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	if (!sngisdn_info->suInstId || !sngisdn_info->spInstId) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Sending PROGRESS, but no call data, aborting (suId:%d suInstId:%u spInstId:%u)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);
		sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
		return;
	}	
	
	if (signal_data->switchtype == SNGISDN_SWITCH_INSNET) {
		/* Trillium Q931 layer complains of invalid event when receiving PROGRESS in
			INSNET variant, so PROGRESS event is probably invalid */
		return;
	}

	memset(&cnStEvnt, 0, sizeof(cnStEvnt));	
	set_prog_ind_ie(ftdmchan, &cnStEvnt.progInd, prog_ind);
	set_facility_ie(ftdmchan, &cnStEvnt.facilityStr);

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending PROGRESS (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces);
	if(sng_isdn_con_status(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId,&cnStEvnt, MI_PROGRESS, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused PROGRESS request\n");
	}
	return;
}

void sngisdn_snd_alert(ftdm_channel_t *ftdmchan, ftdm_sngisdn_progind_t prog_ind)
{
	CnStEvnt cnStEvnt;
	
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	if (!sngisdn_info->suInstId || !sngisdn_info->spInstId) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Sending ALERT, but no call data, aborting (suId:%d suInstId:%u spInstId:%u)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);
		sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
		return;
	}	

	memset(&cnStEvnt, 0, sizeof(cnStEvnt));

	set_prog_ind_ie(ftdmchan, &cnStEvnt.progInd, prog_ind);
	set_facility_ie(ftdmchan, &cnStEvnt.facilityStr);

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending ALERT (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces);

	if(sng_isdn_con_status(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId,&cnStEvnt, MI_ALERTING, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused ALERT request\n");
	}
	return;
}

void sngisdn_snd_connect(ftdm_channel_t *ftdmchan)
{
	CnStEvnt cnStEvnt;
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;
	ftdm_sngisdn_progind_t prog_ind = {SNGISDN_PROGIND_LOC_USER, SNGISDN_PROGIND_DESCR_NETE_ISDN};

	if (sngisdn_test_flag(sngisdn_info, FLAG_SENT_CONNECT)) {
		return;
	}
	sngisdn_set_flag(sngisdn_info, FLAG_SENT_CONNECT);

	if (!sngisdn_info->suInstId || !sngisdn_info->spInstId) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Sending CONNECT, but no call data, aborting (suId:%d suInstId:%u spInstId:%u)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);
		sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
		return;
	}
	
	memset(&cnStEvnt, 0, sizeof(cnStEvnt));

	/* Indicate channel ID only in first response */
	if (!ftdm_test_flag(sngisdn_info, FLAG_SENT_CHAN_ID)) {
		set_chan_id_ie(ftdmchan, &cnStEvnt.chanId);
	}
	set_prog_ind_ie(ftdmchan, &cnStEvnt.progInd, prog_ind);
	set_facility_ie(ftdmchan, &cnStEvnt.facilityStr);

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending CONNECT (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces);
	if (sng_isdn_con_response(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, &cnStEvnt, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "stack refused CONNECT request\n");
	}
	return;
}

void sngisdn_snd_fac_req(ftdm_channel_t *ftdmchan)
{
	FacEvnt facEvnt;
	
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	if (!sngisdn_info->suInstId) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Sending FACILITY, but no call data, ignoring (suId:%d suInstId:%u spInstId:%u)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);
		return;
	}
		
	memset(&facEvnt, 0, sizeof(facEvnt));
	
	if (set_facility_ie_str(ftdmchan, &facEvnt.facElmt.facStr.val[2], (uint8_t*)&facEvnt.facElmt.facStr.len) != FTDM_SUCCESS) {
		/* No point in sending a FACILITY message if there is no Facility IE to transmit */
		return;
	}
	
	facEvnt.facElmt.eh.pres = PRSNT_NODEF;
	facEvnt.facElmt.facStr.pres = PRSNT_NODEF;
	facEvnt.facElmt.facStr.val[0] = 0x1C;
	facEvnt.facElmt.facStr.val[1] = (uint8_t)facEvnt.facElmt.facStr.len;
	facEvnt.facElmt.facStr.len +=2; /* Need to include the size of identifier + len */

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending FACILITY (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces);

	if (sng_isdn_facility_request(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, &facEvnt, MI_FACIL, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused FACILITY request\n");
	}
	return;
}

/* This is used to request Q.921 to initiate link establishment */
void sngisdn_snd_dl_req(ftdm_channel_t *ftdmchan)
{
	CnStEvnt cnStEvnt;
	
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	memset(&cnStEvnt, 0, sizeof(cnStEvnt));

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Requesting Link establishment (suId:%d dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces);

	if (sng_isdn_con_status(signal_data->cc_id, 0, 0, &cnStEvnt, MI_INFO, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused Link establishment\n");
	}
	return;
}

void sngisdn_snd_notify_req(ftdm_channel_t *ftdmchan)
{
	CnStEvnt cnStEvnt;
	
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	if (!sngisdn_info->suInstId || !sngisdn_info->spInstId) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Sending NOTIFY, but no call data, aborting (suId:%d suInstId:%u spInstId:%u)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);
		sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
		return;
	}

	memset(&cnStEvnt, 0, sizeof(cnStEvnt));

	set_not_ind_ie(ftdmchan, &cnStEvnt.notInd);

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending NOTIFY (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces);

	if(sng_isdn_con_status(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId,&cnStEvnt, MI_NOTIFY, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused NOTIFY request\n");
	}
	return;
}


void sngisdn_snd_status_enq(ftdm_channel_t *ftdmchan)
{
	StaEvnt staEvnt;

	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	//ftdm_log_chan_msg(ftdmchan, FTDM_LOG_INFO, "Sending STATUS ENQ\n");

	memset(&staEvnt, 0, sizeof(StaEvnt));
	
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Sending Status ENQ on suId:%d suInstId:%u spInstId:%d dchan:%d ces:%d\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, sngisdn_dchan(signal_data)->link_id, sngisdn_info->ces);
	if (sng_isdn_status_request(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, &staEvnt, MI_STATENQ)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused Status ENQ request\n");
	}
	return;
}


void sngisdn_snd_disconnect(ftdm_channel_t *ftdmchan)
{	
	DiscEvnt discEvnt;

	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	if (!sngisdn_info->suInstId || !sngisdn_info->spInstId) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Sending DISCONNECT, but no call data, aborting (suId:%d suInstId:%u spInstId:%u)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);

		sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
		return;
	}

	memset(&discEvnt, 0, sizeof(discEvnt));

	set_cause_ie(ftdmchan, &discEvnt.causeDgn[0]);
	set_facility_ie(ftdmchan, &discEvnt.facilityStr);
	set_user_to_user_ie(ftdmchan, &discEvnt.usrUsr);

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending DISCONNECT (suId:%d suInstId:%u spInstId:%u)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);
	if (sng_isdn_disc_request(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, &discEvnt)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "stack refused DISCONNECT request\n");
	}
	return;
}

void sngisdn_snd_release(ftdm_channel_t *ftdmchan, uint8_t glare)
{
	RelEvnt relEvnt;
	uint32_t suInstId, spInstId;

	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

 	if (!sngisdn_info->suInstId) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Sending RELEASE, but no call data, aborting (suId:%d suInstId:%u spInstId:%u)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);

		sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP_COMPLETE);
		return;
	}
	
	memset(&relEvnt, 0, sizeof(relEvnt));
	
	/* Fill relEvnt here */
  	relEvnt.causeDgn[0].eh.pres = PRSNT_NODEF;
	relEvnt.causeDgn[0].location.pres = PRSNT_NODEF;
	relEvnt.causeDgn[0].location.val = IN_LOC_PRIVNETLU;
	relEvnt.causeDgn[0].codeStand3.pres = PRSNT_NODEF;
	relEvnt.causeDgn[0].codeStand3.val = IN_CSTD_CCITT;

	relEvnt.causeDgn[0].causeVal.pres = PRSNT_NODEF;
	relEvnt.causeDgn[0].causeVal.val = ftdmchan->caller_data.hangup_cause;
	relEvnt.causeDgn[0].recommend.pres = NOTPRSNT;
	relEvnt.causeDgn[0].dgnVal.pres = NOTPRSNT;

	if (glare) {
		suInstId = sngisdn_info->glare.suInstId;
		spInstId = sngisdn_info->glare.spInstId;
	} else {
		suInstId = sngisdn_info->suInstId;
		spInstId = sngisdn_info->spInstId;
	}

	set_facility_ie(ftdmchan, &relEvnt.facilityStr);
	
	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending RELEASE/RELEASE COMPLETE (suId:%d suInstId:%u spInstId:%u)\n", signal_data->cc_id, suInstId, spInstId);

	if (glare) {
		if (sng_isdn_release_request(signal_data->cc_id, suInstId, spInstId, &relEvnt)) {
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "stack refused RELEASE/RELEASE COMPLETE request\n");
		}
	} else {	
		if (sng_isdn_release_request(signal_data->cc_id, suInstId, spInstId, &relEvnt)) {
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "stack refused RELEASE/RELEASE COMPLETE request\n");
		}
	}	
	return;
}

void sngisdn_snd_restart(ftdm_channel_t *ftdmchan)
{
	Rst rstEvnt;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	memset(&rstEvnt, 0, sizeof(rstEvnt));

	set_chan_id_ie(ftdmchan, &rstEvnt.chanId);
	set_restart_ind_ie(ftdmchan, &rstEvnt.rstInd);
	
	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending RESTART (suId:%d dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_dchan(signal_data)->link_id, CES_MNGMNT);

	if (sng_isdn_restart_request(signal_data->cc_id, &rstEvnt, sngisdn_dchan(signal_data)->link_id, CES_MNGMNT, IN_SND_RST)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "stack refused RESTART request\n");
	}
	return;
}


/* We received an incoming frame on the d-channel, send data to the stack */
void sngisdn_snd_data(ftdm_channel_t *dchan, uint8_t *data, ftdm_size_t len)
{
	sng_l1_frame_t l1_frame;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) dchan->span->signal_data;

	if (len > sizeof(l1_frame.data)) {
		ftdm_log_chan(dchan, FTDM_LOG_ERROR, "Received frame of %"FTDM_SIZE_FMT" bytes, exceeding max size of %"FTDM_SIZE_FMT" bytes\n", 
				len, sizeof(l1_frame.data));
		return;
	}
	if (len <= 2) {
		return;
	}

	memset(&l1_frame, 0, sizeof(l1_frame));
	l1_frame.len = len;

	memcpy(&l1_frame.data, data, len);

	if (ftdm_test_flag(&(dchan->iostats.rx), FTDM_IOSTATS_ERROR_CRC)) {
		l1_frame.flags |= SNG_L1FRAME_ERROR_CRC;
	}

	if (ftdm_test_flag(&(dchan->iostats.rx), FTDM_IOSTATS_ERROR_FRAME)) {
		l1_frame.flags |= SNG_L1FRAME_ERROR_FRAME;
	}
	
	if (ftdm_test_flag(&(dchan->iostats.rx), FTDM_IOSTATS_ERROR_ABORT)) {
		l1_frame.flags |= SNG_L1FRAME_ERROR_ABORT;
	}

	if (ftdm_test_flag(&(dchan->iostats.rx), FTDM_IOSTATS_ERROR_FIFO)) {
		l1_frame.flags |= SNG_L1FRAME_ERROR_FIFO;
	}

	if (ftdm_test_flag(&(dchan->iostats.rx), FTDM_IOSTATS_ERROR_DMA)) {
		l1_frame.flags |= SNG_L1FRAME_ERROR_DMA;
	}

	if (ftdm_test_flag(&(dchan->iostats.rx), FTDM_IOSTATS_ERROR_QUEUE_THRES)) {
		/* Should we trigger congestion here? */		
		l1_frame.flags |= SNG_L1FRAME_QUEUE_THRES;
	}

	if (ftdm_test_flag(&(dchan->iostats.rx), FTDM_IOSTATS_ERROR_QUEUE_FULL)) {
		/* Should we trigger congestion here? */
		l1_frame.flags |= SNG_L1FRAME_QUEUE_FULL;
	}
#if 0
	if (1) {
		int i;
		char string [2000];
		unsigned string_len = 0;
		for (i = 0; i < l1_frame.len; i++) {
			string_len += sprintf(&string[string_len], "0x%02x ", l1_frame.data[i]);
		}

		ftdm_log_chan(dchan, FTDM_LOG_CRIT, "\nL1 RX [%s] flags:%x\n", string, l1_frame.flags);
	}
#endif
	sng_isdn_data_ind(signal_data->link_id, &l1_frame);
}

void sngisdn_snd_event(sngisdn_span_data_t *signal_data, ftdm_oob_event_t event)
{
	sng_l1_event_t l1_event;

	if (!signal_data->dchan) {
		return;
	}
	memset(&l1_event, 0, sizeof(l1_event));

	switch(event) {
		case FTDM_OOB_ALARM_CLEAR:
			l1_event.type = SNG_L1EVENT_ALARM_OFF;
			sng_isdn_event_ind(signal_data->link_id, &l1_event);
			
			ftdm_sched_timer(signal_data->sched, "delayed_dl_req", 8000, sngisdn_delayed_dl_req, (void*) signal_data, NULL);
			signal_data->dl_request_pending = 1;
			break;
		case FTDM_OOB_ALARM_TRAP:
			l1_event.type = SNG_L1EVENT_ALARM_ON;
			sng_isdn_event_ind(signal_data->link_id, &l1_event);
			break;
		default:
			/* We do not care about the other OOB events for now */
			return;
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
