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

void stack_resp_hdr_init(Header *hdr);

ftdm_status_t sngisdn_activate_phy(ftdm_span_t *span);
ftdm_status_t sngisdn_deactivate_phy(ftdm_span_t *span);

ftdm_status_t sngisdn_activate_cc(ftdm_span_t *span);

ftdm_status_t sngisdn_cntrl_q931(ftdm_span_t *span, uint8_t action, uint8_t subaction);
ftdm_status_t sngisdn_cntrl_q921(ftdm_span_t *span, uint8_t action, uint8_t subaction);


extern ftdm_sngisdn_data_t	g_sngisdn_data;

ftdm_status_t sngisdn_stack_stop(ftdm_span_t *span);


ftdm_status_t sngisdn_stack_start(ftdm_span_t *span)
{
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;

	if (signal_data->dchan) {		
		if (sngisdn_cntrl_q921(span, ABND_ENA, NOTUSED) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_CRIT, "%s:Failed to activate stack q921\n", span->name);
			return FTDM_FAIL;
		}
		ftdm_log(FTDM_LOG_DEBUG, "%s:Stack q921 activated\n", span->name);
	}

	/* Try to find an alternative for this */
	/* LAPD will call LdUiDatBndCfm before it received a LdLiMacBndCfm from L1,
	so we need to give some time before activating q931, as q931 will send a
	LdUiDatConReq when activated, and this requires the Mac SAP to be already
	bound first */
	ftdm_sleep(500); 
		
	if (!g_sngisdn_data.ccs[signal_data->cc_id].activation_done) {
		g_sngisdn_data.ccs[signal_data->cc_id].activation_done = 1;
		if (sngisdn_activate_cc(span) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_CRIT, "%s:Failed to activate stack CC\n", span->name);
			return FTDM_FAIL;
		}
		ftdm_log(FTDM_LOG_DEBUG, "%s:Stack CC activated\n", span->name);
	}

	if (sngisdn_cntrl_q931(span, ABND_ENA, SAELMNT) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "%s:Failed to activate stack q931\n", span->name);
		return FTDM_FAIL;
	}
	ftdm_log(FTDM_LOG_DEBUG, "%s:Stack q931 activated\n", span->name);

	ftdm_log(FTDM_LOG_INFO, "%s:Stack activated\n",span->name);
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_stack_stop(ftdm_span_t *span)
{
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;
	/* Stop L1 first, so we do not receive any more frames */
	if (!signal_data->dchan) {
		return FTDM_SUCCESS;
	}
	if (sngisdn_deactivate_phy(span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "%s:Failed to deactivate stack phy\n", span->name);
		return FTDM_FAIL;
	}

	if (sngisdn_cntrl_q931(span, AUBND_DIS, SAELMNT) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "%s:Failed to deactivate stack q931\n", span->name);
		return FTDM_FAIL;
	}

	if (sngisdn_cntrl_q921(span, AUBND_DIS, SAELMNT) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "%s:Failed to deactivate stack q921\n", span->name);
		return FTDM_FAIL;
	}
	
	ftdm_log(FTDM_LOG_INFO, "%s:Signalling stopped\n", span->name);
	return FTDM_SUCCESS;
}


ftdm_status_t sngisdn_activate_phy(ftdm_span_t *span)
{

	/* There is no need to start phy, as it will Q921 will send a activate request to phy when it starts */
	
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_deactivate_phy(ftdm_span_t *span)
{
	L1Mngmt cntrl;
	Pst pst;

	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;

	/* initalize the post structure */
	stack_pst_init(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTL1;

	/* initalize the control structure */
	memset(&cntrl, 0, sizeof(cntrl));

	/* initalize the control header */
	stack_hdr_init(&cntrl.hdr);

	cntrl.hdr.msgType = TCNTRL;			/* configuration */
	cntrl.hdr.entId.ent = ENTL1;		/* entity */
	cntrl.hdr.entId.inst = S_INST;		/* instance */
	cntrl.hdr.elmId.elmnt = STTSAP;		/* SAP Specific cntrl */

	cntrl.t.cntrl.action = AUBND_DIS;
	cntrl.t.cntrl.subAction = SAELMNT;

	cntrl.t.cntrl.sapId = signal_data->link_id;
	
	if (sng_isdn_phy_cntrl(&pst, &cntrl)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_wake_up_phy(ftdm_span_t *span)
{
	L1Mngmt cntrl;
	Pst pst;

	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;

	/* initalize the post structure */
	stack_pst_init(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTL1;

	/* initalize the control structure */
	memset(&cntrl, 0, sizeof(cntrl));

	/* initalize the control header */
	stack_hdr_init(&cntrl.hdr);

	cntrl.hdr.msgType = TCNTRL;			/* configuration */
	cntrl.hdr.entId.ent = ENTL1;		/* entity */
	cntrl.hdr.entId.inst = S_INST;		/* instance */
	cntrl.hdr.elmId.elmnt = STTSAP;		/* SAP Specific cntrl */

	cntrl.t.cntrl.action = AENA;
	cntrl.t.cntrl.subAction = SAELMNT;

	cntrl.t.cntrl.sapId = signal_data->link_id;
	
	if (sng_isdn_phy_cntrl(&pst, &cntrl)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_activate_cc(ftdm_span_t *span)
{
	CcMngmt cntrl;
    Pst pst;

	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;

    /* initalize the post structure */
   stack_pst_init(&pst);

    /* insert the destination Entity */
    pst.dstEnt = ENTCC;

    /* initalize the control structure */
	memset(&cntrl, 0, sizeof(cntrl));

    /* initalize the control header */
    stack_hdr_init(&cntrl.hdr);

	cntrl.hdr.msgType = TCNTRL;			/* configuration */
	cntrl.hdr.entId.ent = ENTCC;		/* entity */
	cntrl.hdr.entId.inst = S_INST;		/* instance */
	cntrl.hdr.elmId.elmnt = STTSAP;		/* physical sap */

	cntrl.t.cntrl.action = ABND_ENA;
	cntrl.t.cntrl.subAction = SAELMNT;

  	cntrl.t.cntrl.sapId = signal_data->cc_id;
	if (sng_isdn_cc_cntrl(&pst, &cntrl)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

ftdm_status_t sngisdn_activate_trace(ftdm_span_t *span, sngisdn_tracetype_t trace_opt)
{
	sngisdn_span_data_t *signal_data = sngisdn_dchan((sngisdn_span_data_t*)span->signal_data);

	if (!signal_data) {
		ftdm_log(FTDM_LOG_ERROR, "%s:Span is not used by signalling module\n", span->name);
		return FTDM_FAIL;
	}

	switch (trace_opt) {
		case SNGISDN_TRACE_DISABLE:
			if (sngisdn_test_trace_flag(signal_data, SNGISDN_TRACE_Q921)) {
				ftdm_log(FTDM_LOG_INFO, "%s:Disabling q921 trace\n", signal_data->ftdm_span->name);
				sngisdn_clear_trace_flag(signal_data, SNGISDN_TRACE_Q921);
				
				if (sngisdn_cntrl_q921(signal_data->ftdm_span, ADISIMM, SATRC) != FTDM_SUCCESS) {
					ftdm_log(FTDM_LOG_INFO, "%s:Failed to disable q921 trace\n", signal_data->ftdm_span->name);
				}
			}
			if (sngisdn_test_trace_flag(signal_data, SNGISDN_TRACE_Q931)) {
				ftdm_log(FTDM_LOG_INFO, "%s:Disabling q921 trace\n", signal_data->ftdm_span->name);
				sngisdn_clear_trace_flag(signal_data, SNGISDN_TRACE_Q931);

				if (sngisdn_cntrl_q931(signal_data->ftdm_span, ADISIMM, SATRC) != FTDM_SUCCESS) {
					ftdm_log(FTDM_LOG_INFO, "%s:Failed to disable q921 trace\n", signal_data->ftdm_span->name);
				}
			}
			break;
		case SNGISDN_TRACE_Q921:
			if (!sngisdn_test_trace_flag(signal_data, SNGISDN_TRACE_Q921)) {
				ftdm_log(FTDM_LOG_INFO, "%s:Enabling q921 trace\n", signal_data->ftdm_span->name);
				sngisdn_set_trace_flag(signal_data, SNGISDN_TRACE_Q921);

				if (sngisdn_cntrl_q921(signal_data->ftdm_span, AENA, SATRC) != FTDM_SUCCESS) {
					ftdm_log(FTDM_LOG_INFO, "%s:Failed to enable q921 trace\n", signal_data->ftdm_span->name);
				}
			}
			break;
		case SNGISDN_TRACE_Q931:
			if (!sngisdn_test_trace_flag(signal_data, SNGISDN_TRACE_Q931)) {
				ftdm_log(FTDM_LOG_INFO, "s%d Enabling q931 trace\n", signal_data->link_id);
				sngisdn_set_trace_flag(signal_data, SNGISDN_TRACE_Q931);
				
				if (sngisdn_cntrl_q931(signal_data->ftdm_span, AENA, SATRC) != FTDM_SUCCESS) {
					ftdm_log(FTDM_LOG_INFO, "%s:Failed to enable q931 trace\n", signal_data->ftdm_span->name);
				}
			}
			break;
	}
	return FTDM_SUCCESS;
}


ftdm_status_t sngisdn_cntrl_q931(ftdm_span_t *span, uint8_t action, uint8_t subaction)
{
	InMngmt cntrl;
	Pst pst;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;

	/* initalize the post structure */
	stack_pst_init(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTIN;

	/* initalize the control structure */
	memset(&cntrl, 0, sizeof(cntrl));

	/* initalize the control header */
	stack_hdr_init(&cntrl.hdr);

	cntrl.hdr.msgType = TCNTRL;			/* configuration */
	cntrl.hdr.entId.ent = ENTIN;		/* entity */
	cntrl.hdr.entId.inst = S_INST;		/* instance */
	cntrl.hdr.elmId.elmnt = STDLSAP;	/* physical sap */

	cntrl.t.cntrl.action = action;
	cntrl.t.cntrl.subAction = subaction;

	if (action == AENA && subaction == SATRC) {
		cntrl.t.cntrl.trcLen = -1; /* Trace the entire message buffer */
	}

	cntrl.t.cntrl.sapId = signal_data->link_id;
	cntrl.t.cntrl.ces = 0;

	if(sng_isdn_q931_cntrl(&pst, &cntrl)) {
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;	

}

ftdm_status_t sngisdn_cntrl_q921(ftdm_span_t *span, uint8_t action, uint8_t subaction)
{
	BdMngmt cntrl;
	Pst pst;
	sngisdn_span_data_t *signal_data = (sngisdn_span_data_t*)span->signal_data;

	/* initalize the post structure */
	stack_pst_init(&pst);

	/* insert the destination Entity */
	pst.dstEnt = ENTLD;

	/* initalize the control structure */
	memset(&cntrl, 0, sizeof(cntrl));

	/* initalize the control header */
	stack_hdr_init(&cntrl.hdr);
	/* build control request */
	cntrl.hdr.msgType          = TCNTRL;
	cntrl.hdr.entId.ent        = ENTLD;
	cntrl.hdr.entId.inst       = S_INST;

#if (SMBD_LMINT3 || BD_LMINT3)
	stack_resp_hdr_init(&cntrl.hdr);
#endif /* _LMINT3 */

	cntrl.hdr.elmId.elmnt      = STMSAP;
	cntrl.t.cntrl.action       = action;
	cntrl.t.cntrl.subAction    = subaction;

#if (SMBD_LMINT3 || BD_LMINT3)
	cntrl.t.cntrl.lnkNmb       = signal_data->link_id;
	cntrl.t.cntrl.sapi         = NOTUSED;
	cntrl.t.cntrl.tei          = NOTUSED;
#else /* _LMINT3 */
	cntrl.hdr.elmId.elmntInst1 = signal_data->link_id;
	cntrl.hdr.elmId.elmntInst2 = NOTUSED;
	cntrl.hdr.elmId.elmntInst3 = NOTUSED;
#endif /* _LMINT3 */

	cntrl.t.cntrl.logInt       = NOTUSED;
	cntrl.t.cntrl.trcLen       = NOTUSED;
	if (action == AENA && subaction == SATRC) {
		cntrl.t.cntrl.trcLen = -1; /* Trace the entire message buffer */
	}

	SGetDateTime(&(cntrl.t.cntrl.dt));
	if(sng_isdn_q921_cntrl(&pst, &cntrl)) {
		return FTDM_FAIL;
	}

	return FTDM_SUCCESS;
}


void stack_resp_hdr_init(Header *hdr)
{ 
	hdr->response.selector   = 0;
	hdr->response.mem.region = RTESPEC;
	hdr->response.mem.pool   = S_POOL;
	hdr->response.prior      = PRIOR0;
	hdr->response.route      = RTESPEC;

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
