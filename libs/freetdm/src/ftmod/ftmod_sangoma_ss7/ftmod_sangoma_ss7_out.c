/*
 * Copyright (c) 2009, Konrad Hammel <konrad@sangoma.com>
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

/* INCLUDE ********************************************************************/
#include "ftmod_sangoma_ss7_main.h"
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/

/* GLOBALS ********************************************************************/
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
void ft_to_sngss7_iam(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_acm(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_anm(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_rel(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_rlc(ftdm_channel_t *ftdmchan);

void ft_to_sngss7_rsc(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_rsca(ftdm_channel_t *ftdmchan);

void ft_to_sngss7_blo(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_bla(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_ubl(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_uba(ftdm_channel_t *ftdmchan);

void ft_to_sngss7_lpa(ftdm_channel_t *ftdmchan);

/* FUNCTIONS ******************************************************************/
void ft_to_sngss7_iam(ftdm_channel_t *ftdmchan)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;;
    SiConEvnt           iam;

    sngss7_info->suInstId = get_unique_id();
    sngss7_info->spInstId = 0;
    sngss7_info->spId = 1;

    memset(&iam, 0x0, sizeof(iam));

    /* copy down the nature of connection indicators */
    iam.natConInd.eh.pres              = PRSNT_NODEF;
    iam.natConInd.satInd.pres          = PRSNT_NODEF;
    iam.natConInd.satInd.val           = 0;
    iam.natConInd.contChkInd.pres      = PRSNT_NODEF;
    iam.natConInd.contChkInd.val       = 0x00;
    iam.natConInd.echoCntrlDevInd.pres = PRSNT_NODEF;
    iam.natConInd.echoCntrlDevInd.val  = 0x01;

    /* copy down the forward call indicators */
    iam.fwdCallInd.eh.pres                 = PRSNT_NODEF;
    iam.fwdCallInd.natIntCallInd.pres      = PRSNT_NODEF;
    iam.fwdCallInd.natIntCallInd.val       = 0x00;
    iam.fwdCallInd.end2EndMethInd.pres     = PRSNT_NODEF;
    iam.fwdCallInd.end2EndMethInd.val      = 0x00;
    iam.fwdCallInd.intInd.pres             = PRSNT_NODEF;
    iam.fwdCallInd.intInd.val              = 0x01;
    iam.fwdCallInd.end2EndInfoInd.pres     = PRSNT_NODEF;
    iam.fwdCallInd.end2EndInfoInd.val      = 0x00;
    iam.fwdCallInd.isdnUsrPrtInd.pres      = PRSNT_NODEF;
    iam.fwdCallInd.isdnUsrPrtInd.val       = 0x01;
    iam.fwdCallInd.isdnUsrPrtPrfInd.pres   = PRSNT_NODEF;
    iam.fwdCallInd.isdnUsrPrtPrfInd.val    = 0x02;
    iam.fwdCallInd.isdnAccInd.pres         = PRSNT_NODEF;
    iam.fwdCallInd.isdnAccInd.val          = 0x01;
    iam.fwdCallInd.sccpMethInd.pres        = PRSNT_NODEF;
    iam.fwdCallInd.sccpMethInd.val         = 0x00;

    /* copy down the calling number information */
    iam.cgPtyCat.eh.pres       = PRSNT_NODEF;
    iam.cgPtyCat.cgPtyCat.pres = PRSNT_NODEF;
    iam.cgPtyCat.cgPtyCat.val  = 0x0a;

    /* copy down the transmission medium requirements */
    iam.txMedReq.eh.pres       = PRSNT_NODEF;
    iam.txMedReq.trMedReq.pres = PRSNT_NODEF;
    iam.txMedReq.trMedReq.val  = 0;    /* SPEECH = 0, 3.1Khz = 3, 64k unres = 2 */

    /* copy down the called number information */
    copy_cdPtyNum_to_sngss7(&ftdmchan->caller_data, &iam.cdPtyNum);

    /* copy down the calling number information */
    copy_cgPtyNum_to_sngss7(&ftdmchan->caller_data, &iam.cgPtyNum);

    sng_cc_con_request(sngss7_info->spId, 
                        sngss7_info->suInstId, 
                        sngss7_info->spInstId, 
                        sngss7_info->circuit->id, 
                        &iam, 
                        0);

    SS7_MSG_TRACE("Transmitted IAM on CIC # %d\n", sngss7_info->circuit->cic);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}
/******************************************************************************/

void ft_to_sngss7_acm(ftdm_channel_t *ftdmchan)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;
    SiCnStEvnt          acm;

    memset(&acm, 0x0, sizeof(acm));

    /* fill in the needed information for the ACM */
    acm.bckCallInd.eh.pres              = PRSNT_NODEF;
    acm.bckCallInd.chrgInd.pres         = PRSNT_NODEF;
    acm.bckCallInd.chrgInd.val          = 0x00;
    acm.bckCallInd.cadPtyStatInd.pres   = PRSNT_NODEF;
    acm.bckCallInd.cadPtyStatInd.val    = 0x01;
    acm.bckCallInd.cadPtyCatInd.pres    = PRSNT_NODEF;
    acm.bckCallInd.cadPtyCatInd.val     = 0x00;
    acm.bckCallInd.end2EndMethInd.pres  = PRSNT_NODEF;
    acm.bckCallInd.end2EndMethInd.val   = 0x00;
    acm.bckCallInd.intInd.pres          = PRSNT_NODEF;
    acm.bckCallInd.intInd.val           = 0x00;
    acm.bckCallInd.end2EndInfoInd.pres  = PRSNT_NODEF;
    acm.bckCallInd.end2EndInfoInd.val   = 0x00;
    acm.bckCallInd.isdnUsrPrtInd.pres   = PRSNT_NODEF;
    acm.bckCallInd.isdnUsrPrtInd.val    = 0x0;
    acm.bckCallInd.holdInd.pres         = PRSNT_NODEF;
    acm.bckCallInd.holdInd.val          = 0x00;
    acm.bckCallInd.isdnAccInd.pres      = PRSNT_NODEF;
    acm.bckCallInd.isdnAccInd.val       = 0x0;
    acm.bckCallInd.echoCtrlDevInd.pres  = PRSNT_NODEF;
    acm.bckCallInd.echoCtrlDevInd.val   = 0x0;
    acm.bckCallInd.sccpMethInd.pres     = PRSNT_NODEF;
    acm.bckCallInd.sccpMethInd.val      = 0x00;

    /* send the ACM request to LibSngSS7 */
    sng_cc_con_status(1, 
                      sngss7_info->suInstId, 
                      sngss7_info->spInstId, 
                      sngss7_info->circuit->id,
                      &acm, 
                      ADDRCMPLT);

    SS7_MSG_TRACE("Transmitted ACM on CIC # %d\n", sngss7_info->circuit->cic);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}
/******************************************************************************/

void ft_to_sngss7_anm(ftdm_channel_t *ftdmchan)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;
    SiConEvnt           anm;

    memset(&anm, 0x0, sizeof(anm));

    /* send the ANM request to LibSngSS7 */
    sng_cc_con_response(1, 
                        sngss7_info->suInstId, 
                        sngss7_info->spInstId, 
                        sngss7_info->circuit->id, 
                        &anm, 
                        5);

    SS7_MSG_TRACE("Transmitted ANM on CIC # %d\n", sngss7_info->circuit->cic);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void ft_to_sngss7_rel(ftdm_channel_t *ftdmchan)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;
    SiRelEvnt           rel;

    memset(&rel, 0x0, sizeof(rel));

    rel.causeDgn.eh.pres           = PRSNT_NODEF;
    rel.causeDgn.location.pres     = PRSNT_NODEF;
    rel.causeDgn.location.val      = 0x01;
    rel.causeDgn.cdeStand.pres     = PRSNT_NODEF;
    rel.causeDgn.cdeStand.val      = 0x00;
    rel.causeDgn.recommend.pres    = NOTPRSNT;
    rel.causeDgn.causeVal.pres     = PRSNT_NODEF;
    rel.causeDgn.causeVal.val      = (uint8_t)ftdmchan->caller_data.hangup_cause;
    rel.causeDgn.dgnVal.pres       = NOTPRSNT;

    /* send the REL request to LibSngSS7 */
    sng_cc_rel_request(1, 
                       sngss7_info->suInstId, 
                       sngss7_info->spInstId, 
                       sngss7_info->circuit->id, 
                       &rel);

    SS7_MSG_TRACE("Transmitted REL on CIC # %d\n", sngss7_info->circuit->cic);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void ft_to_sngss7_rlc(ftdm_channel_t *ftdmchan)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;
    SiRelEvnt           rlc;

    memset(&rlc, 0x0, sizeof(rlc));

    /* send the RLC request to LibSngSS7 */
    sng_cc_rel_response(1, 
                        sngss7_info->suInstId, 
                        sngss7_info->spInstId, 
                        sngss7_info->circuit->id, 
                        &rlc);

    SS7_MSG_TRACE("Transmitted RLC on CIC # %d\n", sngss7_info->circuit->cic);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void ft_to_sngss7_rsc(ftdm_channel_t *ftdmchan)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;

    sng_cc_sta_request(1, 
                        sngss7_info->suInstId, 
                        sngss7_info->spInstId, 
                        sngss7_info->circuit->id, 
                        sngss7_info->globalFlg, 
                        SIT_STA_CIRRESREQ, 
                        NULL);

    SS7_MSG_TRACE("Transmitted RSC on CIC # %d\n", sngss7_info->circuit->cic);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void ft_to_sngss7_rsca(ftdm_channel_t *ftdmchan)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;

    sng_cc_sta_request(1, 
                        sngss7_info->suInstId, 
                        sngss7_info->spInstId, 
                        sngss7_info->circuit->id, 
                        sngss7_info->globalFlg, 
                        SIT_STA_CIRRESRSP, 
                        NULL);

    SS7_MSG_TRACE("Transmitted RSC-RLC on CIC # %d\n", sngss7_info->circuit->cic);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void ft_to_sngss7_blo(ftdm_channel_t *ftdmchan)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;

    sng_cc_sta_request(1, 
                       0, 
                       0, 
                       sngss7_info->circuit->id, 
                       sngss7_info->globalFlg, 
                       SIT_STA_CIRBLOREQ, 
                       NULL);

    SS7_MSG_TRACE("Transmitted BLO on CIC # %d\n", sngss7_info->circuit->cic);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void ft_to_sngss7_bla(ftdm_channel_t *ftdmchan)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;

    sng_cc_sta_request(1, 
                       0, 
                       0, 
                       sngss7_info->circuit->id, 
                       sngss7_info->globalFlg, 
                       SIT_STA_CIRBLORSP, 
                       NULL);

    SS7_MSG_TRACE("Transmitted BLA on CIC # %d\n", sngss7_info->circuit->cic);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void ft_to_sngss7_ubl(ftdm_channel_t *ftdmchan)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;

    sng_cc_sta_request(1, 
                       0, 
                       0, 
                       sngss7_info->circuit->id, 
                       sngss7_info->globalFlg, 
                       SIT_STA_CIRUBLREQ, 
                       NULL);

    SS7_MSG_TRACE("Transmitted UBL on CIC # %d\n", sngss7_info->circuit->cic);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void ft_to_sngss7_uba(ftdm_channel_t *ftdmchan)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;

    sng_cc_sta_request(1, 
                       0, 
                       0, 
                       sngss7_info->circuit->id, 
                       sngss7_info->globalFlg, 
                       SIT_STA_CIRUBLRSP, 
                       NULL);

    SS7_MSG_TRACE("Transmitted UBA on CIC # %d\n", sngss7_info->circuit->cic);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void ft_to_sngss7_lpa(ftdm_channel_t *ftdmchan)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = ftdmchan->call_data;

    sng_cc_sta_request(1, 
                       sngss7_info->suInstId, 
                       sngss7_info->spInstId, 
                       sngss7_info->circuit->id, 
                       sngss7_info->globalFlg, 
                       SIT_STA_LOOPBACKACK, 
                       NULL);

    SS7_MSG_TRACE("Transmitted LPA on CIC # %d\n", sngss7_info->circuit->cic);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}
/******************************************************************************/
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

