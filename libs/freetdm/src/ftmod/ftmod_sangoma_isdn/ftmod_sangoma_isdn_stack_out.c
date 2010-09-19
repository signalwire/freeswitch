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

extern ftdm_status_t cpy_calling_num_from_user(CgPtyNmb *cgPtyNmb, ftdm_caller_data_t *ftdm);
extern ftdm_status_t cpy_called_num_from_user(CdPtyNmb *cdPtyNmb, ftdm_caller_data_t *ftdm);
extern ftdm_status_t cpy_calling_name_from_user(ConEvnt *conEvnt, ftdm_channel_t *ftdmchan);

void sngisdn_snd_setup(ftdm_channel_t *ftdmchan);
void sngisdn_snd_proceed(ftdm_channel_t *ftdmchan);
void sngisdn_snd_progress(ftdm_channel_t *ftdmchan);
void sngisdn_snd_connect(ftdm_channel_t *ftdmchan);
void sngisdn_snd_disconnect(ftdm_channel_t *ftdmchan);
void sngisdn_snd_release(ftdm_channel_t *ftdmchan, uint8_t glare);


void sngisdn_snd_setup(ftdm_channel_t *ftdmchan)
{
	ConEvnt conEvnt;
	sngisdn_chan_data_t *sngisdn_info = ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;	

	ftdm_assert((!sngisdn_info->suInstId && !sngisdn_info->spInstId), "Trying to call out, but call data was not cleared\n");
	
	sngisdn_info->suInstId = get_unique_suInstId(signal_data->cc_id);
	sngisdn_info->spInstId = 0;

	ftdm_mutex_lock(g_sngisdn_data.ccs[signal_data->cc_id].mutex);
	g_sngisdn_data.ccs[signal_data->cc_id].active_suInstIds[sngisdn_info->suInstId] = sngisdn_info;
	ftdm_mutex_unlock(g_sngisdn_data.ccs[signal_data->cc_id].mutex);

	memset(&conEvnt, 0, sizeof(conEvnt));

	conEvnt.bearCap[0].eh.pres = PRSNT_NODEF;
	conEvnt.bearCap[0].infoTranCap.pres = PRSNT_NODEF;
	conEvnt.bearCap[0].infoTranCap.val = sngisdn_get_infoTranCap_from_user(ftdmchan->caller_data.bearer_capability);

	conEvnt.bearCap[0].codeStand0.pres = PRSNT_NODEF;
	conEvnt.bearCap[0].codeStand0.val = IN_CSTD_CCITT;
	conEvnt.bearCap[0].infoTranRate0.pres = PRSNT_NODEF;
	conEvnt.bearCap[0].infoTranRate0.val = IN_ITR_64KBIT;
	conEvnt.bearCap[0].tranMode.pres = PRSNT_NODEF;
	conEvnt.bearCap[0].tranMode.val = IN_TM_CIRCUIT;

	conEvnt.chanId.eh.pres = PRSNT_NODEF;
	conEvnt.chanId.prefExc.pres = PRSNT_NODEF;
	conEvnt.chanId.prefExc.val = IN_PE_EXCLSVE;
	conEvnt.chanId.dChanInd.pres = PRSNT_NODEF;
	conEvnt.chanId.dChanInd.val = IN_DSI_NOTDCHAN;
	conEvnt.chanId.intIdentPres.pres = PRSNT_NODEF;
	conEvnt.chanId.intIdentPres.val = IN_IIP_IMPLICIT;
	conEvnt.chanId.intIdent.pres = NOTPRSNT;

	if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI ||
		ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP) {
		/* Trillium stack rejests lyr1Ident on BRI, but Netbricks always sends it.
		Check with Trillium if this ever causes calls to fail in the field */

		/* BRI only params */
		conEvnt.chanId.intType.pres = PRSNT_NODEF;
		conEvnt.chanId.intType.val = IN_IT_BASIC;
		conEvnt.chanId.infoChanSel.pres = PRSNT_NODEF;
		conEvnt.chanId.infoChanSel.val = ftdmchan->physical_chan_id;
	} else {
		/* PRI only params */
		conEvnt.bearCap[0].usrInfoLyr1Prot.pres = PRSNT_NODEF;
		conEvnt.bearCap[0].usrInfoLyr1Prot.val = sngisdn_get_usrInfoLyr1Prot_from_user(ftdmchan->caller_data.bearer_layer1);
		
		if (signal_data->switchtype == SNGISDN_SWITCH_EUROISDN &&
			conEvnt.bearCap[0].usrInfoLyr1Prot.val == IN_UIL1_G711ULAW) {
			
			/* We are bridging a call from T1 */
			conEvnt.bearCap[0].usrInfoLyr1Prot.val = IN_UIL1_G711ALAW;
			
		} else if (conEvnt.bearCap[0].usrInfoLyr1Prot.val == IN_UIL1_G711ALAW) {
			
			/* We are bridging a call from E1 */
			conEvnt.bearCap[0].usrInfoLyr1Prot.val = IN_UIL1_G711ULAW;
		}
		
		conEvnt.bearCap[0].lyr1Ident.pres = PRSNT_NODEF;
		conEvnt.bearCap[0].lyr1Ident.val = IN_L1_IDENT;

		conEvnt.chanId.intType.pres = PRSNT_NODEF;
		conEvnt.chanId.intType.val = IN_IT_OTHER;
		conEvnt.chanId.infoChanSel.pres = PRSNT_NODEF;
		conEvnt.chanId.infoChanSel.val = IN_ICS_B1CHAN;
		conEvnt.chanId.chanMapType.pres = PRSNT_NODEF;
		conEvnt.chanId.chanMapType.val = IN_CMT_BCHAN;
		conEvnt.chanId.nmbMap.pres = PRSNT_NODEF;
		conEvnt.chanId.nmbMap.val = IN_NM_CHNNMB;
		conEvnt.chanId.codeStand1.pres = PRSNT_NODEF;
		conEvnt.chanId.codeStand1.val = IN_CSTD_CCITT;
		conEvnt.chanId.chanNmbSlotMap.pres = PRSNT_NODEF;
		conEvnt.chanId.chanNmbSlotMap.len = 1;
		conEvnt.chanId.chanNmbSlotMap.val[0] = ftdmchan->physical_chan_id;
	}

	conEvnt.progInd.eh.pres = PRSNT_NODEF;
	conEvnt.progInd.location.pres = PRSNT_NODEF;
	conEvnt.progInd.location.val = IN_LOC_USER;
	conEvnt.progInd.codeStand0.pres = PRSNT_NODEF;
	conEvnt.progInd.codeStand0.val = IN_CSTD_CCITT;
	conEvnt.progInd.progDesc.pres = PRSNT_NODEF;
	conEvnt.progInd.progDesc.val = IN_PD_NOTETEISDN; /* Not end-to-end ISDN */

	if (signal_data->switchtype == SNGISDN_SWITCH_EUROISDN) {
		conEvnt.sndCmplt.eh.pres = PRSNT_NODEF;
	}
	if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP &&
		signal_data->signalling == SNGISDN_SIGNALING_NET) {
		sngisdn_info->ces = CES_MNGMNT;
	}

	cpy_called_num_from_user(&conEvnt.cdPtyNmb, &ftdmchan->caller_data);
	cpy_calling_num_from_user(&conEvnt.cgPtyNmb, &ftdmchan->caller_data);
	cpy_calling_name_from_user(&conEvnt, ftdmchan);

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending SETUP (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, signal_data->dchan_id, sngisdn_info->ces);

	if (sng_isdn_con_request(signal_data->cc_id, sngisdn_info->suInstId, &conEvnt, signal_data->dchan_id, sngisdn_info->ces)) {
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

	cnStEvnt.chanId.eh.pres = PRSNT_NODEF;
	cnStEvnt.chanId.prefExc.pres = PRSNT_NODEF;
	cnStEvnt.chanId.prefExc.val = IN_PE_EXCLSVE;
	cnStEvnt.chanId.dChanInd.pres = PRSNT_NODEF;
	cnStEvnt.chanId.dChanInd.val = IN_DSI_NOTDCHAN;
	cnStEvnt.chanId.intIdentPres.pres = PRSNT_NODEF;
	cnStEvnt.chanId.intIdentPres.val = IN_IIP_IMPLICIT;

	
	if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI ||
		ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP) {

		/* BRI only params */
		cnStEvnt.chanId.intType.pres = PRSNT_NODEF;
		cnStEvnt.chanId.intType.val = IN_IT_BASIC;
		cnStEvnt.chanId.infoChanSel.pres = PRSNT_NODEF;
		cnStEvnt.chanId.infoChanSel.val = ftdmchan->physical_chan_id;
	} else {
		cnStEvnt.chanId.intType.pres = PRSNT_NODEF;
		cnStEvnt.chanId.intType.val = IN_IT_OTHER;
		cnStEvnt.chanId.infoChanSel.pres = PRSNT_NODEF;
		cnStEvnt.chanId.infoChanSel.val = IN_ICS_B1CHAN;
		cnStEvnt.chanId.chanMapType.pres = PRSNT_NODEF;
		cnStEvnt.chanId.chanMapType.val = IN_CMT_BCHAN;
		cnStEvnt.chanId.nmbMap.pres = PRSNT_NODEF;
		cnStEvnt.chanId.nmbMap.val = IN_NM_CHNNMB;
		cnStEvnt.chanId.codeStand1.pres = PRSNT_NODEF;
		cnStEvnt.chanId.codeStand1.val = IN_CSTD_CCITT;
		cnStEvnt.chanId.chanNmbSlotMap.pres = PRSNT_NODEF;
		cnStEvnt.chanId.chanNmbSlotMap.len = 1;
		cnStEvnt.chanId.chanNmbSlotMap.val[0] = ftdmchan->physical_chan_id;
	}

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending SETUP ACK (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, signal_data->dchan_id, sngisdn_info->ces);

	if(sng_isdn_con_status(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, &cnStEvnt, MI_SETUPACK, signal_data->dchan_id, sngisdn_info->ces)) {
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
	
	cnStEvnt.chanId.eh.pres = PRSNT_NODEF;
	cnStEvnt.chanId.prefExc.pres = PRSNT_NODEF;
	cnStEvnt.chanId.prefExc.val = IN_PE_EXCLSVE;
	cnStEvnt.chanId.dChanInd.pres = PRSNT_NODEF;
	cnStEvnt.chanId.dChanInd.val = IN_DSI_NOTDCHAN;
	cnStEvnt.chanId.intIdentPres.pres = PRSNT_NODEF;
	cnStEvnt.chanId.intIdentPres.val = IN_IIP_IMPLICIT;
	
	if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI ||
		ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP) {

		/* BRI only params */
		cnStEvnt.chanId.intType.pres = PRSNT_NODEF;
		cnStEvnt.chanId.intType.val = IN_IT_BASIC;
		cnStEvnt.chanId.infoChanSel.pres = PRSNT_NODEF;
		cnStEvnt.chanId.infoChanSel.val = ftdmchan->physical_chan_id;
	} else {
		cnStEvnt.chanId.intType.pres = PRSNT_NODEF;
		cnStEvnt.chanId.intType.val = IN_IT_OTHER;
		cnStEvnt.chanId.infoChanSel.pres = PRSNT_NODEF;
		cnStEvnt.chanId.infoChanSel.val = IN_ICS_B1CHAN;
		cnStEvnt.chanId.chanMapType.pres = PRSNT_NODEF;
		cnStEvnt.chanId.chanMapType.val = IN_CMT_BCHAN;
		cnStEvnt.chanId.nmbMap.pres = PRSNT_NODEF;
		cnStEvnt.chanId.nmbMap.val = IN_NM_CHNNMB;
		cnStEvnt.chanId.codeStand1.pres = PRSNT_NODEF;
		cnStEvnt.chanId.codeStand1.val = IN_CSTD_CCITT;
		cnStEvnt.chanId.chanNmbSlotMap.pres = PRSNT_NODEF;
		cnStEvnt.chanId.chanNmbSlotMap.len = 1;
		cnStEvnt.chanId.chanNmbSlotMap.val[0] = ftdmchan->physical_chan_id;
	}


	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending CONNECT COMPL (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, signal_data->dchan_id, sngisdn_info->ces);

	if(sng_isdn_con_comp(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, &cnStEvnt, signal_data->dchan_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused CONNECT ACK request\n");
	}
	return;
}


void sngisdn_snd_proceed(ftdm_channel_t *ftdmchan)
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
	
	memset(&cnStEvnt, 0, sizeof(cnStEvnt));
	
	cnStEvnt.chanId.eh.pres = PRSNT_NODEF;
	cnStEvnt.chanId.prefExc.pres = PRSNT_NODEF;
	cnStEvnt.chanId.prefExc.val = IN_PE_EXCLSVE;
	cnStEvnt.chanId.dChanInd.pres = PRSNT_NODEF;
	cnStEvnt.chanId.dChanInd.val = IN_DSI_NOTDCHAN;
	cnStEvnt.chanId.intIdentPres.pres = PRSNT_NODEF;
	cnStEvnt.chanId.intIdentPres.val = IN_IIP_IMPLICIT;
	
	if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI ||
		ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP) {

		/* BRI only params */
		cnStEvnt.chanId.intType.pres = PRSNT_NODEF;
		cnStEvnt.chanId.intType.val = IN_IT_BASIC;
		cnStEvnt.chanId.infoChanSel.pres = PRSNT_NODEF;
		cnStEvnt.chanId.infoChanSel.val = ftdmchan->physical_chan_id;
	} else {
		cnStEvnt.chanId.intType.pres = PRSNT_NODEF;
		cnStEvnt.chanId.intType.val = IN_IT_OTHER;
		cnStEvnt.chanId.infoChanSel.pres = PRSNT_NODEF;
		cnStEvnt.chanId.infoChanSel.val = IN_ICS_B1CHAN;
		cnStEvnt.chanId.chanMapType.pres = PRSNT_NODEF;
		cnStEvnt.chanId.chanMapType.val = IN_CMT_BCHAN;
		cnStEvnt.chanId.nmbMap.pres = PRSNT_NODEF;
		cnStEvnt.chanId.nmbMap.val = IN_NM_CHNNMB;
		cnStEvnt.chanId.codeStand1.pres = PRSNT_NODEF;
		cnStEvnt.chanId.codeStand1.val = IN_CSTD_CCITT;
		cnStEvnt.chanId.chanNmbSlotMap.pres = PRSNT_NODEF;
		cnStEvnt.chanId.chanNmbSlotMap.len = 1;
		cnStEvnt.chanId.chanNmbSlotMap.val[0] = ftdmchan->physical_chan_id;
	}


	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending PROCEED (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, signal_data->dchan_id, sngisdn_info->ces);

	if(sng_isdn_con_status(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, &cnStEvnt, MI_CALLPROC, signal_data->dchan_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused PROCEED request\n");
	}
	return;
}

void sngisdn_snd_progress(ftdm_channel_t *ftdmchan)
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

	cnStEvnt.progInd.eh.pres = PRSNT_NODEF;
	cnStEvnt.progInd.location.pres = PRSNT_NODEF;
	cnStEvnt.progInd.location.val = IN_LOC_USER;
	cnStEvnt.progInd.codeStand0.pres = PRSNT_NODEF;
	cnStEvnt.progInd.codeStand0.val = IN_CSTD_CCITT;
	cnStEvnt.progInd.progDesc.pres = PRSNT_NODEF;
	cnStEvnt.progInd.progDesc.val = IN_PD_IBAVAIL;

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending PROGRESS (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, signal_data->dchan_id, sngisdn_info->ces);
	if(sng_isdn_con_status(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId,&cnStEvnt, MI_PROGRESS, signal_data->dchan_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused PROGRESS request\n");
	}
	return;
}

void sngisdn_snd_alert(ftdm_channel_t *ftdmchan)
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

	cnStEvnt.progInd.eh.pres = PRSNT_NODEF;
	cnStEvnt.progInd.location.pres = PRSNT_NODEF;
	cnStEvnt.progInd.location.val = IN_LOC_USER;
	cnStEvnt.progInd.codeStand0.pres = PRSNT_NODEF;
	cnStEvnt.progInd.codeStand0.val = IN_CSTD_CCITT;
	cnStEvnt.progInd.progDesc.pres = PRSNT_NODEF;
	cnStEvnt.progInd.progDesc.val = IN_PD_NOTETEISDN;

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending ALERT (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, signal_data->dchan_id, sngisdn_info->ces);

	if(sng_isdn_con_status(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId,&cnStEvnt, MI_ALERTING, signal_data->dchan_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused ALERT request\n");
	}
	return;
}

void sngisdn_snd_connect(ftdm_channel_t *ftdmchan)
{
	CnStEvnt cnStEvnt;
	
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

 	if (!sngisdn_info->suInstId || !sngisdn_info->spInstId) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Sending CONNECT, but no call data, aborting (suId:%d suInstId:%u spInstId:%u)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId);
		sngisdn_set_flag(sngisdn_info, FLAG_LOCAL_ABORT);
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
		return;
	}
	
	memset(&cnStEvnt, 0, sizeof(cnStEvnt));


	cnStEvnt.chanId.eh.pres = PRSNT_NODEF;
	cnStEvnt.chanId.prefExc.pres = PRSNT_NODEF;
	cnStEvnt.chanId.prefExc.val = IN_PE_EXCLSVE;
	cnStEvnt.chanId.dChanInd.pres = PRSNT_NODEF;
	cnStEvnt.chanId.dChanInd.val = IN_DSI_NOTDCHAN;
	cnStEvnt.chanId.intIdentPres.pres = PRSNT_NODEF;
	cnStEvnt.chanId.intIdentPres.val = IN_IIP_IMPLICIT;
	
	if (ftdmchan->span->trunk_type == FTDM_TRUNK_BRI ||
		ftdmchan->span->trunk_type == FTDM_TRUNK_BRI_PTMP) {

		/* BRI only params */
		cnStEvnt.chanId.intType.pres = PRSNT_NODEF;
		cnStEvnt.chanId.intType.val = IN_IT_BASIC;
		cnStEvnt.chanId.infoChanSel.pres = PRSNT_NODEF;
		cnStEvnt.chanId.infoChanSel.val = ftdmchan->physical_chan_id;
	} else {
		cnStEvnt.chanId.intType.pres = PRSNT_NODEF;
		cnStEvnt.chanId.intType.val = IN_IT_OTHER;
		cnStEvnt.chanId.infoChanSel.pres = PRSNT_NODEF;
		cnStEvnt.chanId.infoChanSel.val = IN_ICS_B1CHAN;
		cnStEvnt.chanId.chanMapType.pres = PRSNT_NODEF;
		cnStEvnt.chanId.chanMapType.val = IN_CMT_BCHAN;
		cnStEvnt.chanId.nmbMap.pres = PRSNT_NODEF;
		cnStEvnt.chanId.nmbMap.val = IN_NM_CHNNMB;
		cnStEvnt.chanId.codeStand1.pres = PRSNT_NODEF;
		cnStEvnt.chanId.codeStand1.val = IN_CSTD_CCITT;
		cnStEvnt.chanId.chanNmbSlotMap.pres = PRSNT_NODEF;
		cnStEvnt.chanId.chanNmbSlotMap.len = 1;
		cnStEvnt.chanId.chanNmbSlotMap.val[0] = ftdmchan->physical_chan_id;
	}
	
	cnStEvnt.progInd.eh.pres = PRSNT_NODEF;
	cnStEvnt.progInd.location.pres = PRSNT_NODEF;
	cnStEvnt.progInd.location.val = IN_LOC_USER;
	cnStEvnt.progInd.codeStand0.pres = PRSNT_NODEF;
	cnStEvnt.progInd.codeStand0.val = IN_CSTD_CCITT;
	cnStEvnt.progInd.progDesc.pres = PRSNT_NODEF;
	cnStEvnt.progInd.progDesc.val = IN_PD_NOTETEISDN; /* Not end-to-end ISDN */

	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending CONNECT (suId:%d suInstId:%u spInstId:%u dchan:%d ces:%d)\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, signal_data->dchan_id, sngisdn_info->ces);
	if (sng_isdn_con_response(signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, &cnStEvnt, signal_data->dchan_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "stack refused CONNECT request\n");
	}
	return;
}


void sngisdn_snd_info_req(ftdm_channel_t *ftdmchan)
{
	CnStEvnt cnStEvnt;
	
	sngisdn_chan_data_t *sngisdn_info = (sngisdn_chan_data_t*) ftdmchan->call_data;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*) ftdmchan->span->signal_data;

	if (ftdmchan->span->trunk_type != FTDM_TRUNK_BRI &&
		ftdmchan->span->trunk_type != FTDM_TRUNK_BRI_PTMP) {

		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Ignoring INFO REQ on non-BRI channel\n");
		return;
	}

	memset(&cnStEvnt, 0, sizeof(cnStEvnt));
	//ftdm_log_chan_msg(ftdmchan, FTDM_LOG_INFO, "Sending INFO REQ\n");


	ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "Sending INFO REQ (suId:%d dchan:%d ces:%d)\n", signal_data->cc_id, signal_data->dchan_id, sngisdn_info->ces);

	if (sng_isdn_con_status(signal_data->cc_id, 0, 0, &cnStEvnt, MI_INFO, signal_data->dchan_id, sngisdn_info->ces)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, 	"stack refused INFO request\n");
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

	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Sending Status ENQ on suId:%d suInstId:%u spInstId:%d dchan:%d ces:%d\n", signal_data->cc_id, sngisdn_info->suInstId, sngisdn_info->spInstId, signal_data->dchan_id, sngisdn_info->ces);
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
	
	/* Fill discEvnt here */	
  	discEvnt.causeDgn[0].eh.pres = PRSNT_NODEF;
	discEvnt.causeDgn[0].location.pres = PRSNT_NODEF;
	discEvnt.causeDgn[0].location.val = IN_LOC_PRIVNETLU;
	discEvnt.causeDgn[0].codeStand3.pres = PRSNT_NODEF;
	discEvnt.causeDgn[0].codeStand3.val = IN_CSTD_CCITT;
	discEvnt.causeDgn[0].causeVal.pres = PRSNT_NODEF;
	discEvnt.causeDgn[0].causeVal.val = ftdmchan->caller_data.hangup_cause;
	discEvnt.causeDgn[0].recommend.pres = NOTPRSNT;
	discEvnt.causeDgn[0].dgnVal.pres = NOTPRSNT;

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
	
	/* Fill discEvnt here */	
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
