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
void sngss7_sta_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
void sngss7_con_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
void sngss7_con_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
void sngss7_con_sta(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiCnStEvnt *siCnStEvnt, uint8_t evntType);
void sngss7_rel_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
void sngss7_rel_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
void sngss7_dat_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiInfoEvnt *siInfoEvnt);
void sngss7_fac_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
void sngss7_fac_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
void sngss7_umsg_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit);

static ftdm_status_t handle_reattempt(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
static ftdm_status_t handle_pause(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
static ftdm_status_t handle_resume(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);

static ftdm_status_t handle_cot_start(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
static ftdm_status_t handle_cot_stop(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
static ftdm_status_t handle_cot(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);

static ftdm_status_t handle_rsc_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
static ftdm_status_t handle_local_rsc_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
static ftdm_status_t handle_rsc_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);

static ftdm_status_t handle_blo_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
static ftdm_status_t handle_blo_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
static ftdm_status_t handle_ubl_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
static ftdm_status_t handle_ubl_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);



/******************************************************************************/

/* FUNCTIONS ******************************************************************/
void sngss7_con_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info ;
    ftdm_channel_t      *ftdmchan;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    };

    SS7_MSG_TRACE("Received IAM on CIC # %d\n", sngss7_info->circuit->cic);

    /* check whether the ftdm channel is in a state to accept a call */
    switch (ftdmchan->state) {
    /**************************************************************************/
    case (FTDM_CHANNEL_STATE_DOWN):     /* only state it is fully valid to get IAM */

        /* fill in the channels SS7 Stack information */
        sngss7_info->suInstId = get_unique_id();
        sngss7_info->spInstId = spInstId;

        /* try to open the ftdm channel */
        if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
            SS7_ERROR("Failed to open span: %d, chan: %d\n",
                        ftdmchan->physical_span_id,
                        ftdmchan->physical_chan_id);

             /* set the flag to indicate this hangup is started from the local side */
            sngss7_set_flag(sngss7_info, FLAG_LOCAL_REL);

            ftdmchan->caller_data.hangup_cause = 41;

            /* move the state to CANCEL */
            ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_CANCEL);

        } else {

            /* fill in cid/ani number */
            if (siConEvnt->cgPtyNum.addrSig.pres) {
                copy_tknStr_from_sngss7(siConEvnt->cgPtyNum.addrSig,
                                        ftdmchan->caller_data.cid_num.digits, 
                                        siConEvnt->cgPtyNum.oddEven);

                /* fill in cid Name */
                ftdm_set_string(ftdmchan->caller_data.cid_name, ftdmchan->caller_data.cid_num.digits);

                ftdm_set_string(ftdmchan->caller_data.ani.digits, ftdmchan->caller_data.cid_num.digits);

            } else {
                SS7_INFO("No Calling party (ANI) information in IAM!\n");
            }

            /* fill in dnis */
            if (siConEvnt->cdPtyNum.addrSig.pres) {
                copy_tknStr_from_sngss7(siConEvnt->cdPtyNum.addrSig, 
                                        ftdmchan->caller_data.dnis.digits, 
                                        siConEvnt->cdPtyNum.oddEven);
            }   else {
                SS7_INFO("No Called party (DNIS) information in IAM!\n");
            }

            /* fill in rdnis */
            if (siConEvnt->redirgNum.addrSig.pres) {
                copy_tknStr_from_sngss7(siConEvnt->redirgNum.addrSig, 
                                        ftdmchan->caller_data.rdnis.digits, 
                                        siConEvnt->cgPtyNum.oddEven);
            }   else {
                SS7_INFO("No RDNIS party information in IAM!\n");
            }

            /* fill in screening/presentation */
            ftdmchan->caller_data.screen = siConEvnt->cgPtyNum.scrnInd.val;
            ftdmchan->caller_data.pres = siConEvnt->cgPtyNum.presRest.val;

            /* set the state of the channel to collecting...the rest is done by the chan monitor */
            ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_COLLECT);

        } /* if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) */

        break;
    /**************************************************************************/
    case (FTDM_CHANNEL_STATE_DIALING):             /* glare */
        SS7_ERROR("Got IAM in DIALING state...glare...queueing incoming call\n");
        
        /* the flag the channel as having a collision */
        sngss7_set_flag(sngss7_info, FLAG_GLARE);

        /* save the IAM for processing once the channel has gone to DOWN */
        memcpy(&sngss7_info->glare.iam, siConEvnt, sizeof(*siConEvnt));
        sngss7_info->glare.suInstId = suInstId;
        sngss7_info->glare.spInstId = spInstId;
        sngss7_info->glare.circuit  = circuit;

        break;
    /**************************************************************************/
    default:    /* should not have gotten an IAM while in this state */
        SS7_ERROR("Got IAM in an invalid state (%s) on span=%d, chan=%d!\n", 
                    ftdm_channel_state2str(ftdmchan->state),
                    ftdmchan->physical_span_id,
                    ftdmchan->physical_chan_id);

        /* move the state of the channel to RESTART to force a reset */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);

        break;
    /**************************************************************************/
    } /* switch (ftdmchan->state) */

    /* unlock the channel */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void sngss7_con_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info ;
    ftdm_channel_t      *ftdmchan;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    };

    SS7_MSG_TRACE("Received ANM on CIC # %d\n", sngss7_info->circuit->cic);

    /* check whether the ftdm channel is in a state to accept a call */
    switch (ftdmchan->state) {
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_PROGRESS:
    case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:

        /* go to UP */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);

        break;
    /**************************************************************************/
    default:    /* incorrect state...reset the CIC */

        /* go to RESTART */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS);

        break;
    /**************************************************************************/
    }

    /* unlock the channel */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void sngss7_con_sta(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiCnStEvnt *siCnStEvnt, uint8_t evntType)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info ;
    ftdm_channel_t      *ftdmchan;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    }

    switch (evntType) {
    /**************************************************************************/
    case (ADDRCMPLT):
        SS7_MSG_TRACE("Received ACM on CIC # %d\n", sngss7_info->circuit->cic);
        switch (ftdmchan->state) {
        /**********************************************************************/
        case FTDM_CHANNEL_STATE_DIALING:
            /* KONRAD: should we confirm the instance ids ? */

            /* need to grab the sp instance id */ 
            sngss7_info->spInstId = spInstId;

            /* go to PROGRESS */
            ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS);
        break;
        /**********************************************************************/
        default:    /* incorrect state...reset the CIC */
            /* go to RESTART */
            ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS);
        break;
        /**********************************************************************/
        } /* switch (ftdmchan->state) */
    /**************************************************************************/
    case (MODIFY):
        SS7_MSG_TRACE("Received MODIFY on CIC # %d\n", sngss7_info->circuit->cic);
        break;
    /**************************************************************************/
    case (MODCMPLT):
        SS7_MSG_TRACE( "Received MODIFY_COMPLETE on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (MODREJ):
        SS7_MSG_TRACE( "Received MODIFY_REJECT on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (PROGRESS):
        SS7_MSG_TRACE( "Received CPG on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (FRWDTRSFR):
        SS7_MSG_TRACE( "Received FOT on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (INFORMATION):
        SS7_MSG_TRACE( "Received INF on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (INFORMATREQ):
        SS7_MSG_TRACE( "Received INR on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (SUBSADDR):
        SS7_MSG_TRACE( "Received SAM on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (EXIT):
        SS7_MSG_TRACE( "Received EXIT on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (NETRESMGT):
        SS7_MSG_TRACE( "Received NRM on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (IDENTREQ):
        SS7_MSG_TRACE( "Received IDR on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (IDENTRSP):
        SS7_MSG_TRACE( "Received IRS on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (MALCLLPRNT):
        SS7_MSG_TRACE( "Received MALICIOUS CALL on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (CHARGE):
        SS7_MSG_TRACE( "Received CRG on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (TRFFCHGE):
        SS7_MSG_TRACE( "Received CRG-Tariff Change on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (CHARGEACK):
        SS7_MSG_TRACE( "Received CRG-Acknowledge on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (CALLOFFMSG):
        SS7_MSG_TRACE( "Received CALL_OFFER on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (LOOPPRVNT):
        SS7_MSG_TRACE( "Received LOP on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (TECT_TIMEOUT):
        SS7_MSG_TRACE( "Received ECT Timeout on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (RINGSEND):
        SS7_MSG_TRACE( "Received Ringing Send on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (CALLCLEAR):
        SS7_MSG_TRACE( "Received Call_Line_clear on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (PRERELEASE):
        SS7_MSG_TRACE( "Received PRI on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (APPTRANSPORT):
        SS7_MSG_TRACE( "Received APM on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (OPERATOR):
        SS7_MSG_TRACE( "Received Operator on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (METPULSE):
        SS7_MSG_TRACE( "Received Metering Pulse on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (CLGPTCLR):
        SS7_MSG_TRACE( "Received Calling_Party_Clear on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    case (SUBDIRNUM):
        SS7_MSG_TRACE( "Received subsequent directory number on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    default:
        ftdm_log(FTDM_LOG_ERROR, "Received Unknown message on circuit # %d\n", circuit);
        break;
    /**************************************************************************/
    }

    /* unlock the channel */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void sngss7_rel_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info ;
    ftdm_channel_t      *ftdmchan;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    };

    SS7_MSG_TRACE("Received REL on CIC # %d\n", sngss7_info->circuit->cic);

    /* check whether the ftdm channel is in a state to release a call */
    switch (ftdmchan->state) {
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_RING:
    case FTDM_CHANNEL_STATE_DIALING:
    case FTDM_CHANNEL_STATE_PROGRESS:
    case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
    case FTDM_CHANNEL_STATE_UP:

        /* pass the release code up to FTDM */
        if (siRelEvnt->causeDgn.causeVal.pres) {
            ftdmchan->caller_data.hangup_cause = siRelEvnt->causeDgn.causeVal.val;
        } else {
            SS7_ERROR("REL does not have a cause code!\n");
            ftdmchan->caller_data.hangup_cause = 0;
        }

        /* move the state of the channel to TERMINATING to end the call */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);

        break;
    /**************************************************************************/
    default:

        /* fill in the channels SS7 Stack information */
        sngss7_info->suInstId = get_unique_id();
        sngss7_info->spInstId = spInstId;

        /* throw the reset flag */
        sngss7_set_flag(sngss7_info, FLAG_RESET_RX);

        /* set the state to RESTART */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
        break;
    /**************************************************************************/
    } /* switch (ftdmchan->state) */


    /* unlock the channel */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}


/******************************************************************************/
void sngss7_rel_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);
    
    sngss7_chan_data_t  *sngss7_info ;
    ftdm_channel_t      *ftdmchan;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return;
    };

    SS7_MSG_TRACE("Received RLC on CIC # %d\n", sngss7_info->circuit->cic);

    /* check whether the ftdm channel is in a state to accept a call */
    switch (ftdmchan->state) {
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:

        /* go to DOWN */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);

        break;
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_DOWN:
        /* do nothing, just drop the message */
        break;
    /**************************************************************************/
    default:    
        /* KONRAD: should just stop the call...but a reset is easier for now (since it does hangup the call) */

        /* go to RESTART */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);

        break;
    /**************************************************************************/
    }

    /* unlock the channel */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void sngss7_dat_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiInfoEvnt *siInfoEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);
    SS7_MSG_TRACE( "Received DATA indication on circuit # %d\n", circuit);
    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void sngss7_fac_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);
    SS7_MSG_TRACE( "Received FAC request on circuit # %d\n", circuit);
    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void sngss7_fac_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);
    SS7_MSG_TRACE( "Received FAC confirm on circuit # %d\n", circuit);
    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
void sngss7_umsg_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    SS7_MSG_TRACE( "Received User to User message on circuit # %d\n", circuit);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/* GENERAL STATUS *************************************************************/
void sngss7_sta_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    SS7_MSG_TRACE("[SNG-CC] Received %s indicaton on cic = %d\n", 
                DECODE_LCC_EVENT(evntType),
                g_ftdm_sngss7_data.cfg.isupCircuit[circuit].cic);

    switch (evntType) {
    /**************************************************************************/
    case SIT_STA_REATTEMPT:         /* reattempt indication */
        handle_reattempt(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
        break;
    /**************************************************************************/
    case SIT_STA_ERRORIND:          /* error indication */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_CONTCHK:           /* continuity check */
        handle_cot_start(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
        break;
    /**************************************************************************/
    case SIT_STA_CONTREP:           /* continuity report */
        handle_cot(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
        break;
    /**************************************************************************/
    case SIT_STA_STPCONTIN:         /* stop continuity */
        handle_cot_stop(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
        break;
    /**************************************************************************/
    case SIT_STA_CGQRYRSP:          /* circuit grp query response from far end forwarded to upper layer by ISUP */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_CONFUSION:         /* confusion */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_LOOPBACKACK:       /* loop-back acknowledge */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_CIRRSRVREQ:        /* circuit reservation request */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_CIRRSRVACK:        /* circuit reservation acknowledgement */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_CIRBLOREQ:         /* circuit blocking request */
        handle_blo_req(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
        break;
    /**************************************************************************/
    case SIT_STA_CIRBLORSP:         /* circuit blocking response   */
        handle_blo_rsp(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
        break;
    /**************************************************************************/
    case SIT_STA_CIRUBLREQ:         /* circuit unblocking request */
        handle_ubl_req(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
        break;
    /**************************************************************************/
    case SIT_STA_CIRUBLRSP:         /* circuit unblocking response */
        handle_ubl_rsp(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
        break;
    /**************************************************************************/
    case SIT_STA_CIRRESREQ:         /* circuit reset request - RSC */
        handle_rsc_req(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
        break;
    /**************************************************************************/
    case SIT_STA_CIRLOCRES:         /* reset initiated locally by the software */
        handle_local_rsc_rsp(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
        break;
    /**************************************************************************/
    case SIT_STA_CIRRESRSP:         /* circuit reset response */
        handle_rsc_rsp(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
        break;
    /**************************************************************************/
    case SIT_STA_CGBREQ:            /* CGB request */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_CGUREQ:            /* CGU request */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_CGQRYREQ:          /* circuit group query request */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_CGBRSP:            /* mntc. oriented CGB response */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_CGURSP:            /* mntc. oriented CGU response */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_GRSREQ:            /* circuit group reset request */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_CIRUNEQPD:         /* circuit unequipped indication */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_GRSRSP:            /* circuit group reset response */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_PAUSEIND:          /* pause indication */
        handle_pause(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
        break;
    /**************************************************************************/
    case SIT_STA_RESUMEIND:         /* resume indication */
        handle_resume(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
        break;
    /**************************************************************************/
    case SIT_STA_USRPARTA:          /* user part available */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_RMTUSRUNAV:        /* remote user not available */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_MTPCONG0:          /* congestion indication level 0 */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_MTPCONG1:          /* congestion indication level 1 */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_MTPCONG2:          /* congestion indication level 2 */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_MTPCONG3:          /* congestion indication level 3 */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_MTPSTPCONG:        /* stop congestion indication level 0 */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break; 
    /**************************************************************************/
    case SIT_STA_CIRLOCALBLOIND:    /* Mngmt local blocking */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_CIRLOCALUBLIND:    /* Mngmt local unblocking */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_OVERLOAD:          /* Overload */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_LMCGBREQ:          /* when LM requests ckt grp blocking */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_LMCGUREQ:          /* when LM requests ckt grp unblocking */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_LMGRSREQ:          /* when LM requests ckt grp reset */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_CGBINFOIND:        /* circuit grp blking ind , no resp req */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_LMCQMINFOREQ:      /* when LM requests ckt grp query */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_CIRLOCGRS:         /* group reset initiated locally by the software */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_GRSRSPIND:         /* indication to IW to idle the RM */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    case SIT_STA_RLCIND:            /* RLC indicattion to IW to idle the RM */
        SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
        break;
    /**************************************************************************/
    default:
        SS7_INFO("[SNG-CC] Received Unknown indication %d\n", evntType);
        break;
    } /* switch (evntType) */

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}

/******************************************************************************/
static ftdm_status_t handle_reattempt(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for ISUP circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    };

    /* glare, throw the flag, go to down state*/
    sngss7_set_flag(sngss7_info, FLAG_GLARE);

    ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);

    /* unlock the channel again before we exit */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_pause(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;
    int                 infId;
    int                 i;

    /* extract the affect infId from the circuit structure */
    infId = g_ftdm_sngss7_data.cfg.isupCircuit[circuit].infId;

    /* go through all the circuits now and find any other circuits on this infId */
    i = 1;
    while (g_ftdm_sngss7_data.cfg.isupCircuit[i].id != 0) {
        
        /* check that the infId matches and that this is not a siglink */
        if ((g_ftdm_sngss7_data.cfg.isupCircuit[i].infId == infId) && 
            (g_ftdm_sngss7_data.cfg.isupCircuit[i].siglink == 0)) {

            /* get the ftdmchan and ss7_chan_data from the circuit */
            if (extract_chan_data(i, &sngss7_info, &ftdmchan)) {
                SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
                i++;
                continue;
            }

            /* lock the channel */
            ftdm_mutex_lock(ftdmchan->mutex);

            /* check if there is a pending state change, give it a bit to clear */
            if (check_for_state_change(ftdmchan)) {
                SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
                ftdm_mutex_unlock(ftdmchan->mutex);
                i++;
                continue;
            };

            /* set the pause flag on the channel */
            sngss7_set_flag(sngss7_info, FLAG_INFID_PAUSED);

            /* set the statet o SUSPENDED to bring the sig status down */ 
            ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);

            /* unlock the channel again before we exit */
            ftdm_mutex_unlock(ftdmchan->mutex);

        } /* if (g_ftdm_sngss7_data.cfg.isupCircuit[i].infId == infId) */

        /* move to the next circuit */
        i++;

    } /* while (g_ftdm_sngss7_data.cfg.isupCircuit[i].id != 0) */

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_resume(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;
    int                 infId;
    int                 i;

    /* extract the affect infId from the circuit structure */
    infId = g_ftdm_sngss7_data.cfg.isupCircuit[circuit].infId;

    /* go through all the circuits now and find any other circuits on this infId */
    i = 1;
    while (g_ftdm_sngss7_data.cfg.isupCircuit[i].id != 0) {

        /* check that the infId matches and that this is not a siglink */
        if ((g_ftdm_sngss7_data.cfg.isupCircuit[i].infId == infId) && 
            (g_ftdm_sngss7_data.cfg.isupCircuit[i].siglink == 0)) {

            /* get the ftdmchan and ss7_chan_data from the circuit */
            if (extract_chan_data(i, &sngss7_info, &ftdmchan)) {
                SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
                i++;
                continue;
            }

            /* lock the channel */
            ftdm_mutex_lock(ftdmchan->mutex);

            /* check if there is a pending state change, give it a bit to clear */
            if (check_for_state_change(ftdmchan)) {
                SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
                ftdm_mutex_unlock(ftdmchan->mutex);
                i++;
                continue;
            };

            /* set the resume flag on the channel */
            sngss7_set_flag(sngss7_info, FLAG_INFID_RESUME);

            /* clear the paused flag */
            sngss7_clear_flag(sngss7_info, FLAG_INFID_PAUSED);

            /* set the statet to SUSPENDED to bring the sig status up */ 
            ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);

            /* unlock the channel again before we exit */
            ftdm_mutex_unlock(ftdmchan->mutex);

        } /* if (g_ftdm_sngss7_data.cfg.isupCircuit[i].infId == infId) */

        /* move to the next circuit */
        i++;

    } /* while (g_ftdm_sngss7_data.cfg.isupCircuit[i].id != 0) */

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_cot_start(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    };

    /* open the channel if it is not open */
    if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_OPEN)) {
        if (ftdm_channel_open_chan(ftdmchan) != FTDM_SUCCESS) {
            SS7_ERROR("Failed to open CIC %d for COT test!\n", sngss7_info->circuit->cic);
            /* KONRAD FIX ME */
            SS7_FUNC_TRACE_EXIT(__FUNCTION__);
            return FTDM_FAIL;
        }
    }

    /* tell the core to loop the channel */
    ftdm_channel_command(ftdmchan, FTDM_COMMAND_ENABLE_LOOP, NULL);

    /* switch to the IN_LOOP state */
    ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_IN_LOOP);

    /* store the sngss7 ids */
    if (suInstId == 0) {
        sngss7_info->suInstId = get_unique_id();
    } else {
        sngss7_info->suInstId = suInstId;
    }
    sngss7_info->spInstId = spInstId;
    sngss7_info->globalFlg = globalFlg;

    /* unlock the channel again before we exit */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_cot_stop(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    };

    /* tell the core to stop looping the channel */
    ftdm_channel_command(ftdmchan, FTDM_COMMAND_DISABLE_LOOP, NULL);

    /* exit out of the LOOP state to the last state */
    ftdm_set_state_locked(ftdmchan, ftdmchan->last_state);

    /* unlock the channel again before we exit */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_cot(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    if ( (siStaEvnt->contInd.eh.pres > 0) && (siStaEvnt->contInd.contInd.pres > 0)) {
        SS7_INFO("Continuity Test result for CIC = %d (span %d, chan %d) is: \"%s\"\n",
                    g_ftdm_sngss7_data.cfg.isupCircuit[circuit].cic,
                    g_ftdm_sngss7_data.cfg.isupCircuit[circuit].span,
                    g_ftdm_sngss7_data.cfg.isupCircuit[circuit].chan,
                    (siStaEvnt->contInd.contInd.val) ? "PASS" : "FAIL");
    } else {
        SS7_ERROR("Recieved Continuity report containing no results!\n");
    }

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_rsc_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    };

    switch (ftdmchan->state) {
    /**************************************************************************/
    case FTDM_CHANNEL_STATE_RESTART:
        /* since we are already in RESTART, the channel is already free...
         * just waitng for the other side to clean up it's channel.
         * just send an acknowledge to this reset, don't go through the master
         * state handler....bad but...
         */

        sngss7_info->spInstId = 0;
        sngss7_info->suInstId = 0;
        sngss7_info->globalFlg = globalFlg;

        ft_to_sngss7_rsca(ftdmchan);
        break;
    /**************************************************************************/
    default:
        /* throw the channels RESET_RX flag */
        sngss7_set_flag(sngss7_info, FLAG_RESET_RX);

        sngss7_info->spInstId = 0;
        sngss7_info->suInstId = 0;
        sngss7_info->globalFlg = globalFlg;

        /* set the state of the channel to restart...the rest is done by the chan monitor */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
        break;
    /**************************************************************************/
    }

    /* unlock the channel again before we exit */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_local_rsc_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    };

    /* the stack is sending out a RSC for us */
    sngss7_set_flag(sngss7_info, FLAG_RESET_RX);

    sngss7_info->spInstId = 0;
    sngss7_info->suInstId = 0;
    sngss7_info->globalFlg = globalFlg;

    /* set the state of the channel to restart...the rest is done by the chan monitor */
    ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);

    /* unlock the channel again before we exit */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_rsc_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    };

    switch (ftdmchan->state) {
    /**********************************************************************/
    case FTDM_CHANNEL_STATE_RESTART:

        /* go to DOWN */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);

        break;
    /**********************************************************************/
    case FTDM_CHANNEL_STATE_DOWN:
        /* do nothing, just drop the message */
        break;
    /**********************************************************************/
    case FTDM_CHANNEL_STATE_TERMINATING:
    case FTDM_CHANNEL_STATE_HANGUP:
    case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
    /**********************************************************************/
        /* go to DOWN */
        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);

        break;
    /**********************************************************************/
    default:    
        /* ITU Q764-2.9.5.1.c -> release the circuit */
        if ((siStaEvnt != NULL) && 
            (siStaEvnt->causeDgn.eh.pres ==PRSNT_NODEF) && 
            (siStaEvnt->causeDgn.causeVal.pres == PRSNT_NODEF)) {
            ftdmchan->caller_data.hangup_cause = siStaEvnt->causeDgn.causeVal.val;
        } else {
            ftdmchan->caller_data.hangup_cause = 98;    /* Message not compatiable with call state */
        }

        ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
        break;
    /**********************************************************************/
    }

    /* unlock the channel again before we exit */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_blo_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    };

    /* check if the circuit is already blocked or not */
    if (sngss7_test_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX)) {
        SS7_WARN("Received BLO on circuit that is already blocked!\n");
    }

    /* throw the ckt block flag */
    sngss7_set_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX);

    /* set the channel to suspended state */
    ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);

    /* unlock the channel again before we exit */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_blo_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    };

    /* KONRAD FIX ME */

    /* unlock the channel again before we exit */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_ubl_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    };

    /* check if the channel is blocked */
    if (!(sngss7_test_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX))) {
        SS7_WARN("Received UBL on circuit that is not blocked!\n");
    }

    /* throw the unblock flag */
    sngss7_set_flag(sngss7_info, FLAG_CKT_MN_UNBLK_RX);

    /* clear the block flag */
    sngss7_clear_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX);

    /* set the channel to suspended state */
    ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);

    /* unlock the channel again before we exit */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
static ftdm_status_t handle_ubl_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    };

    /* KONRAD FIX ME */

    /* unlock the channel again before we exit */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
#if 0
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_chan_data_t  *sngss7_info = NULL;
    ftdm_channel_t      *ftdmchan = NULL;

    /* get the ftdmchan and ss7_chan_data from the circuit */
    if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
        SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    }

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    /* check if there is a pending state change, give it a bit to clear */
    if (check_for_state_change(ftdmchan)) {
        SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
        ftdm_mutex_unlock(ftdmchan->mutex);
        SS7_FUNC_TRACE_EXIT(__FUNCTION__);
        return FTDM_FAIL;
    };

    /* fill in here */

    /* unlock the channel again before we exit */
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return FTDM_SUCCESS;
}

/******************************************************************************/
#endif
/******************************************************************************/
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

