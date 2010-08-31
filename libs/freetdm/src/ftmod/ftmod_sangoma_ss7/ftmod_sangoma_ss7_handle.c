/*
 * Copyright (c) 2009 Konrad Hammel <konrad@sangoma.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms|with or without
 * modification|are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice|this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice|this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES|INCLUDING|BUT NOT
 * LIMITED TO|THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT|INDIRECT|INCIDENTAL|SPECIAL,
 * EXEMPLARY|OR CONSEQUENTIAL DAMAGES (INCLUDING|BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE|DATA|OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY|WHETHER IN CONTRACT|STRICT LIABILITY|OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE|EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* INCLUDE ********************************************************************/
#include "ftmod_sangoma_ss7_main.h"
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/

/* GLOBALS ********************************************************************/
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
/* PROTOTYPES *****************************************************************/
ftdm_status_t handle_con_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
ftdm_status_t handle_con_sta(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiCnStEvnt *siCnStEvnt, uint8_t evntType);
ftdm_status_t handle_con_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
ftdm_status_t handle_rel_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
ftdm_status_t handle_rel_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
ftdm_status_t handle_dat_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiInfoEvnt *siInfoEvnt);
ftdm_status_t handle_fac_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
ftdm_status_t handle_fac_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
ftdm_status_t handle_umsg_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit);
ftdm_status_t handle_sta_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);

ftdm_status_t handle_reattempt(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_pause(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_resume(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_cot_start(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_cot_stop(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_cot(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_rsc_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_local_rsc_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_rsc_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_grs_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_grs_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_blo_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_blo_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_ubl_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_ubl_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_local_blk(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_local_ubl(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_ucic(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
/******************************************************************************/

/* FUNCTIONS ******************************************************************/
ftdm_status_t handle_con_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info ;
	ftdm_channel_t	  *ftdmchan;

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
		SS7_ASSERT;
	};

	SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx IAM\n");

	/* check if the circuit has a remote block */
	if ((sngss7_test_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX)) ||
		(sngss7_test_flag(sngss7_info, FLAG_GRP_HW_BLOCK_RX)) ||
		(sngss7_test_flag(sngss7_info, FLAG_GRP_MN_BLOCK_RX))) {

		/* as per Q.764, 2.8.2.3 xiv ... remove the block from this channel */
		sngss7_clear_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX);
		sngss7_clear_flag(sngss7_info, FLAG_GRP_HW_BLOCK_RX);
		sngss7_clear_flag(sngss7_info, FLAG_GRP_MN_BLOCK_RX);

		/* KONRAD FIX ME : check in case there is a ckt and grp block */
	}

	/* check whether the ftdm channel is in a state to accept a call */
	switch (ftdmchan->state) {
	/**************************************************************************/
	case (FTDM_CHANNEL_STATE_DOWN):	 /* only state it is fully valid to get IAM */

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
	default:	/* should not have gotten an IAM while in this state */
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
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_con_sta(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiCnStEvnt *siCnStEvnt, uint8_t evntType)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info ;
	ftdm_channel_t	  *ftdmchan;

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
		SS7_ASSERT;
	}

	switch (evntType) {
	/**************************************************************************/
	case (ADDRCMPLT):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx ACM\n");
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
		default:	/* incorrect state...reset the CIC */
			/* go to RESTART */
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_PROGRESS);
		break;
		/**********************************************************************/
		} /* switch (ftdmchan->state) */
	/**************************************************************************/
	case (MODIFY):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx MODIFY\n");
		break;
	/**************************************************************************/
	case (MODCMPLT):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx MODIFY-COMPLETE\n");
		break;
	/**************************************************************************/
	case (MODREJ):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx MODIFY-REJECT\n");
		break;
	/**************************************************************************/
	case (PROGRESS):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CPG\n");
		break;
	/**************************************************************************/
	case (FRWDTRSFR):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx FOT\n");
		break;
	/**************************************************************************/
	case (INFORMATION):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx INF\n");
		break;
	/**************************************************************************/
	case (INFORMATREQ):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx INR\n");
		break;
	/**************************************************************************/
	case (SUBSADDR):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx SAM\n");
		break;
	/**************************************************************************/
	case (EXIT):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx EXIT\n");
		break;
	/**************************************************************************/
	case (NETRESMGT):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx NRM\n");
		break;
	/**************************************************************************/
	case (IDENTREQ):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx IDR\n");
		break;
	/**************************************************************************/
	case (IDENTRSP):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx IRS\n");
		break;
	/**************************************************************************/
	case (MALCLLPRNT):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx MALICIOUS CALL\n");
		break;
	/**************************************************************************/
	case (CHARGE):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CRG\n");
		break;
	/**************************************************************************/
	case (TRFFCHGE):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CRG-TARIFF\n");
		break;
	/**************************************************************************/
	case (CHARGEACK):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CRG-ACK\n");
		break;
	/**************************************************************************/
	case (CALLOFFMSG):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CALL-OFFER\n");
		break;
	/**************************************************************************/
	case (LOOPPRVNT):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx LOP\n");
		break;
	/**************************************************************************/
	case (TECT_TIMEOUT):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx ECT-Timeout\n");
		break;
	/**************************************************************************/
	case (RINGSEND):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx RINGING-SEND\n");
		break;
	/**************************************************************************/
	case (CALLCLEAR):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CALL-LINE Clear\n");
		break;
	/**************************************************************************/
	case (PRERELEASE):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx PRI\n");
		break;
	/**************************************************************************/
	case (APPTRANSPORT):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx APM\n");
		break;
	/**************************************************************************/
	case (OPERATOR):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx OPERATOR\n");
		break;
	/**************************************************************************/
	case (METPULSE):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx METERING-PULSE\n");
		break;
	/**************************************************************************/
	case (CLGPTCLR):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CALLING_PARTY_CLEAR\n");
		break;
	/**************************************************************************/
	case (SUBDIRNUM):
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx SUB-DIR\n");
		break;
	/**************************************************************************/
	default:
	   	SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Unknown Msg\n");
		break;
	/**************************************************************************/
	}

	/* unlock the channel */
	ftdm_mutex_unlock(ftdmchan->mutex);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_con_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info ;
	ftdm_channel_t	  *ftdmchan;

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
		SS7_ASSERT;
	};


	/* check whether the ftdm channel is in a state to accept a call */
	switch (ftdmchan->state) {
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_PROGRESS:
	case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:

		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx ANM\n");

		/* go to UP */
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);

		break;
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_DIALING:

		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CON\n");

		/* go to UP */
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_UP);

		break;		
	/**************************************************************************/
	default:	/* incorrect state...reset the CIC */

		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx ANM/CON\n");

		/* throw the TX reset flag */
		sngss7_set_flag(sngss7_info, FLAG_GRP_RESET_TX);

		/* go to RESTART */
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);

		break;
	/**************************************************************************/
	}

	/* unlock the channel */
	ftdm_mutex_unlock(ftdmchan->mutex);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_rel_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info ;
	ftdm_channel_t	  *ftdmchan;

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
		SS7_ASSERT;
	};

	SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx REL\n");

	/* check whether the ftdm channel is in a state to release a call */
	switch (ftdmchan->state) {
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_DIALING:

		/* pass the release code up to FTDM */
		if (siRelEvnt->causeDgn.causeVal.pres) {
			ftdmchan->caller_data.hangup_cause = siRelEvnt->causeDgn.causeVal.val;
		} else {
			SS7_ERROR("REL does not have a cause code!\n");
			ftdmchan->caller_data.hangup_cause = 0;
		}

		/* this is a remote hangup request */
		sngss7_set_flag(sngss7_info, FLAG_REMOTE_REL);

		/* move the state of the channel to CANCEL to end the call */
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);

		break;
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_RING:
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

		/* this is a remote hangup request */
		sngss7_set_flag(sngss7_info, FLAG_REMOTE_REL);

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
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_rel_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);
	
	sngss7_chan_data_t  *sngss7_info ;
	ftdm_channel_t	  *ftdmchan;

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
		SS7_ASSERT;
	};

	SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx RLC\n");

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
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_dat_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiInfoEvnt *siInfoEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);
	
	sngss7_chan_data_t  *sngss7_info ;
	ftdm_channel_t	  *ftdmchan;

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
		SS7_ASSERT;
	};

	SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx DATA IND\n");

	/* unlock the channel */
	ftdm_mutex_unlock(ftdmchan->mutex);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_fac_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);
	
	sngss7_chan_data_t  *sngss7_info ;
	ftdm_channel_t	  *ftdmchan;

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
		SS7_ASSERT;
	};

	SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx FAC\n");

	/* unlock the channel */
	ftdm_mutex_unlock(ftdmchan->mutex);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_fac_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);
	
	sngss7_chan_data_t  *sngss7_info ;
	ftdm_channel_t	  *ftdmchan;

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
		SS7_ASSERT;
	};

	SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx FAC-CON\n");

	/* unlock the channel */
	ftdm_mutex_unlock(ftdmchan->mutex);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_umsg_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);
	
	sngss7_chan_data_t  *sngss7_info ;
	ftdm_channel_t	  *ftdmchan;

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
		SS7_ASSERT;
	};

	SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx USER-USER msg\n");

	/* unlock the channel */
	ftdm_mutex_unlock(ftdmchan->mutex);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_sta_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	sngss7_chan_data_t  *sngss7_info ;
	ftdm_channel_t	  *ftdmchan;

	/* get the ftdmchan and ss7_chan_data from the circuit */
	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return FTDM_FAIL;
	}

	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	switch (evntType) {
	/**************************************************************************/
	case SIT_STA_REATTEMPT:		 /* reattempt indication */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Reattempt indication\n");
		handle_reattempt(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_ERRORIND:		  /* error indication */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Error indication\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_CONTCHK:		   /* continuity check */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx COT start\n");
		handle_cot_start(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_CONTREP:		   /* continuity report */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx COT report\n");
		handle_cot(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_STPCONTIN:		 /* stop continuity */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx COT stop\n");
		handle_cot_stop(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_CGQRYRSP:		  /* circuit grp query response from far end forwarded to upper layer by ISUP */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CQM\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_CONFUSION:		 /* confusion */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CFN\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_LOOPBACKACK:	   /* loop-back acknowledge */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx LPA\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_CIRRSRVREQ:		/* circuit reservation request */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Ckt Resveration req\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_CIRRSRVACK:		/* circuit reservation acknowledgement */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Ckt Res ack\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_CIRBLOREQ:		 /* circuit blocking request */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx BLO\n");
		handle_blo_req(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_CIRBLORSP:		 /* circuit blocking response   */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx BLA\n");
		handle_blo_rsp(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_CIRUBLREQ:		 /* circuit unblocking request */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx UBL\n");
		handle_ubl_req(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_CIRUBLRSP:		 /* circuit unblocking response */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx UBA\n");
		handle_ubl_rsp(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_CIRRESREQ:		 /* circuit reset request - RSC */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx RSC\n");
		handle_rsc_req(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_CIRLOCRES:		 /* reset initiated locally by the software */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Local RSC\n");
		handle_local_rsc_req(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_CIRRESRSP:		 /* circuit reset response */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx RSC-RLC\n");
		handle_rsc_rsp(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_CGBREQ:			/* CGB request */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CGB\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_CGUREQ:			/* CGU request */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CGU\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_CGQRYREQ:		  /* circuit group query request */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CQM\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_CGBRSP:			/* mntc. oriented CGB response */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx mntc CGB\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_CGURSP:			/* mntc. oriented CGU response */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx mntc CGU\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_GRSREQ:			/* circuit group reset request */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx GRS\n");
		handle_grs_req(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_CIRUNEQPD:		 /* circuit unequipped indication */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx UCIC\n");
		handle_ucic(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_GRSRSP:			/* circuit group reset response */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx GRA\n");
		handle_grs_rsp(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_PAUSEIND:		  /* pause indication */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx SUS\n");
		handle_pause(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_RESUMEIND:		 /* resume indication */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx RES\n");
		handle_resume(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_USRPARTA:		  /* user part available */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx UPA\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_RMTUSRUNAV:		/* remote user not available */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Remote User not Available\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_MTPCONG0:		  /* congestion indication level 0 */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Congestion L0\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_MTPCONG1:		  /* congestion indication level 1 */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Congestion L1\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_MTPCONG2:		  /* congestion indication level 2 */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Congestion L2\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_MTPCONG3:		  /* congestion indication level 3 */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Congestion L3\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_MTPSTPCONG:		/* stop congestion indication level 0 */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Stop Congestion\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break; 
	/**************************************************************************/
	case SIT_STA_CIRLOCALBLOIND:	/* Mngmt local blocking */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Local BLO\n");
		handle_local_blk(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_CIRLOCALUBLIND:	/* Mngmt local unblocking */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Local UBL\n");
		handle_local_ubl(suInstId, spInstId, circuit, globalFlg, evntType, siStaEvnt);
		break;
	/**************************************************************************/
	case SIT_STA_OVERLOAD:		  /* Overload */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Overload\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_LMCGBREQ:		  /* when LM requests ckt grp blocking */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx LM CGB\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_LMCGUREQ:		  /* when LM requests ckt grp unblocking */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx LM CGU\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_LMGRSREQ:		  /* when LM requests ckt grp reset */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx LM RSC\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_CGBINFOIND:		/* circuit grp blking ind , no resp req */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx CGB no resp req\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_LMCQMINFOREQ:	  /* when LM requests ckt grp query */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx LM CQM\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	case SIT_STA_CIRLOCGRS:		 /* group reset initiated locally by the software */
		SS7_MSG_TRACE(ftdmchan, sngss7_info, "Rx Local GRS\n");
		SS7_WARN(" %s indication not currently supported\n", DECODE_LCC_EVENT(evntType));
		break;
	/**************************************************************************/
	default:
		SS7_INFO("[SNG-CC] Received Unknown indication %d\n", evntType);
		break;
	} /* switch (evntType) */

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;

}

/******************************************************************************/
ftdm_status_t handle_reattempt(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;

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
		SS7_ASSERT;
	};

	/* glare, throw the flag, go to down state*/
	sngss7_set_flag(sngss7_info, FLAG_GLARE);

	ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_CANCEL);

	/* unlock the channel again before we exit */
	ftdm_mutex_unlock(ftdmchan->mutex);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_pause(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);
	
	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;
	int				 infId;
	int				 i;
	
	/* extract the affect infId from the circuit structure */
	infId = g_ftdm_sngss7_data.cfg.isupCkt[circuit].infId;
	
	/* go through all the circuits now and find any other circuits on this infId */
	i = 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[i].id != 0) {
		
		/* check that the infId matches and that this is not a siglink */
		if ((g_ftdm_sngss7_data.cfg.isupCkt[i].infId == infId) && 
			(g_ftdm_sngss7_data.cfg.isupCkt[i].type == VOICE)) {
	
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
				SS7_ASSERT;
			};
	
			/* check if the circuit is fully started */
			if (ftdm_test_flag(ftdmchan->span, FTDM_SPAN_IN_THREAD)) {
				/* set the pause flag on the channel */
				sngss7_set_flag(sngss7_info, FLAG_INFID_PAUSED);
	
				/* set the statet o SUSPENDED to bring the sig status down */ 
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
			}
	
			/* unlock the channel again before we exit */
			ftdm_mutex_unlock(ftdmchan->mutex);
	
		} /* if (g_ftdm_sngss7_data.cfg.isupCkt[i].infId == infId) */
	
		/* move to the next circuit */
		i++;
	
	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[i].id != 0) */
	
	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_resume(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;
	int				 infId;
	int				 i;

	/* extract the affect infId from the circuit structure */
	infId = g_ftdm_sngss7_data.cfg.isupCkt[circuit].infId;

	/* go through all the circuits now and find any other circuits on this infId */
	i = 1;
	while (g_ftdm_sngss7_data.cfg.isupCkt[i].id != 0) {

		/* check that the infId matches and that this is not a siglink */
		if ((g_ftdm_sngss7_data.cfg.isupCkt[i].infId == infId) && 
			(g_ftdm_sngss7_data.cfg.isupCkt[i].type == VOICE)) {

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
				SS7_ASSERT;
			};

			/* only resume if we are paused */
			if (sngss7_test_flag(sngss7_info, FLAG_INFID_PAUSED)) {
				/* set the resume flag on the channel */
				sngss7_set_flag(sngss7_info, FLAG_INFID_RESUME);

				/* clear the paused flag */
				sngss7_clear_flag(sngss7_info, FLAG_INFID_PAUSED);

				/* set the statet to SUSPENDED to bring the sig status up */ 
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);
			}
			
			/* unlock the channel again before we exit */
			ftdm_mutex_unlock(ftdmchan->mutex);

		} /* if (g_ftdm_sngss7_data.cfg.isupCkt[i].infId == infId) */

		/* move to the next circuit */
		i++;

	} /* while (g_ftdm_sngss7_data.cfg.isupCkt[i].id != 0) */

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_cot_start(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;

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
		SS7_ASSERT;
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
ftdm_status_t handle_cot_stop(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;

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
		SS7_ASSERT;
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
ftdm_status_t handle_cot(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	if ( (siStaEvnt->contInd.eh.pres > 0) && (siStaEvnt->contInd.contInd.pres > 0)) {
		SS7_INFO("Continuity Test result for CIC = %d (span %d, chan %d) is: \"%s\"\n",
					g_ftdm_sngss7_data.cfg.isupCkt[circuit].cic,
					g_ftdm_sngss7_data.cfg.isupCkt[circuit].span,
					g_ftdm_sngss7_data.cfg.isupCkt[circuit].chan,
					(siStaEvnt->contInd.contInd.val) ? "PASS" : "FAIL");
	} else {
		SS7_ERROR("Recieved Continuity report containing no results!\n");
	}

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/


/******************************************************************************/
ftdm_status_t handle_blo_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;

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
		SS7_ASSERT;
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
ftdm_status_t handle_blo_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;

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
		SS7_ASSERT;
	};

	/* KONRAD FIX ME */

	/* unlock the channel again before we exit */
	ftdm_mutex_unlock(ftdmchan->mutex);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_ubl_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;

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
		SS7_ASSERT;
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
ftdm_status_t handle_ubl_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;

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
		SS7_ASSERT;
	};

	/* KONRAD FIX ME */

	/* unlock the channel again before we exit */
	ftdm_mutex_unlock(ftdmchan->mutex);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_rsc_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;

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
		SS7_ASSERT;
	};

	/* throw the reset flag */
	sngss7_set_flag(sngss7_info, FLAG_RESET_RX);

	switch (ftdmchan->state) {
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_RESTART:

		/* go to idle so that we can redo the restart state*/
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_IDLE);

		break;
	/**************************************************************************/
	default:

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
ftdm_status_t handle_local_rsc_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;

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
		SS7_ASSERT;
	};

	/* throw the reset flag */
	sngss7_set_flag(sngss7_info, FLAG_RESET_RX);

	switch (ftdmchan->state) {
	/**************************************************************************/
	case FTDM_CHANNEL_STATE_RESTART:

		/* go to idle so that we can redo the restart state*/
		ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_IDLE);

		break;
	/**************************************************************************/
	default:

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
ftdm_status_t handle_rsc_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;

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
		SS7_ASSERT;
	};

	switch (ftdmchan->state) {
	/**********************************************************************/
	case FTDM_CHANNEL_STATE_RESTART:
		
		if ( sngss7_test_flag(sngss7_info, FLAG_RESET_TX) ) {
			/* throw the FLAG_RESET_TX_RSP to indicate we have acknowledgement from the remote side */
			sngss7_set_flag(sngss7_info, FLAG_RESET_TX_RSP);

			/* go to DOWN */
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
		} else {
			SS7_ERROR("Received RSC-RLC but we're not waiting on a RSC-RLC on CIC #, dropping\n", sngss7_info->circuit->cic);
		}

		break;
	/**********************************************************************/
	case FTDM_CHANNEL_STATE_DOWN:
		
		/* do nothing, just drop the message */
		SS7_DEBUG("Receveived RSC-RLC in down state, dropping\n");
		
		break;
	/**********************************************************************/
	case FTDM_CHANNEL_STATE_TERMINATING:
	case FTDM_CHANNEL_STATE_HANGUP:
	case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
		
		/* throw the FLAG_RESET_TX_RSP to indicate we have acknowledgement from the remote side */
		sngss7_set_flag(sngss7_info, FLAG_RESET_TX_RSP);

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
			ftdmchan->caller_data.hangup_cause = 98;	/* Message not compatiable with call state */
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
ftdm_status_t handle_grs_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;
	sngss7_span_data_t	*span = NULL; 
	int					range;
	int 				x;

	if (extract_chan_data(circuit, &sngss7_info, &ftdmchan)) {
		SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return FTDM_FAIL;
	}

	/* extract the range value from the event structure */
	if ((siStaEvnt->rangStat.eh.pres == PRSNT_NODEF) && (siStaEvnt->rangStat.range.pres == PRSNT_NODEF)) {
		range = siStaEvnt->rangStat.range.val;
	} else {
		SS7_ERROR("Received GRS with no range value on CIC = %d\n", sngss7_info->circuit->cic);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return FTDM_FAIL;
	}

	/* loop over the cics starting from circuit until range+1 */
	for (x = circuit; x < (circuit + range + 1); x++) {
		/* grab the circuit in question */
		if (extract_chan_data(x, &sngss7_info, &ftdmchan)) {
			SS7_ERROR("Failed to extract channel data for circuit = %d!\n", x);
			break;
		}
	
		/* now that we have the right channel...put a lock on it so no-one else can use it */
		ftdm_mutex_lock(ftdmchan->mutex);
	
		/* check if there is a pending state change, give it a bit to clear */
		if (check_for_state_change(ftdmchan)) {
			SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
			ftdm_mutex_unlock(ftdmchan->mutex);
			SS7_ASSERT;
		};

		/* fill in the span structure for this circuit */
		span = ftdmchan->span->mod_data;
		span->grs.circuit = circuit; 
		span->grs.range = range;

		SS7_DEBUG_CHAN(ftdmchan, "Rx GRS (%d:%d)\n", 
								g_ftdm_sngss7_data.cfg.isupCkt[circuit].cic, 
								(g_ftdm_sngss7_data.cfg.isupCkt[circuit].cic + range));

		/* flag the channel as having received a reset */
		sngss7_set_flag(sngss7_info, FLAG_GRP_RESET_RX);

		switch (ftdmchan->state) {
		/**************************************************************************/
		case FTDM_CHANNEL_STATE_RESTART:

			/* go to idle so that we can redo the restart state*/
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_IDLE);

			break;
		/**************************************************************************/
		default:

			/* set the state of the channel to restart...the rest is done by the chan monitor */
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_RESTART);
			break;
		/**************************************************************************/
		}

		/* unlock the channel again before we exit */
		ftdm_mutex_unlock(ftdmchan->mutex);

	}

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_grs_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;
	int					range;
	int 				x;

	/* extract the range value from the event structure */
	if ((siStaEvnt->rangStat.eh.pres == PRSNT_NODEF) && (siStaEvnt->rangStat.range.pres == PRSNT_NODEF)) {
		range = siStaEvnt->rangStat.range.val;
	} else {
		SS7_ERROR("Received GRA with no range value on CIC = %d\n", sngss7_info->circuit->cic);
		SS7_FUNC_TRACE_EXIT(__FUNCTION__);
		return FTDM_FAIL;
	}

	/* go through all the circuits in the range */
	for ( x = circuit; x < (circuit + range + 1); x++) {

		/* grab the circuit in question */
		if (extract_chan_data(x, &sngss7_info, &ftdmchan)) {
			SS7_ERROR("Failed to extract channel data for circuit = %d!\n", circuit);
			break;
		}

		/* now that we have the right channel...put a lock on it so no-one else can use it */
		ftdm_mutex_lock(ftdmchan->mutex);

		/* check if there is a pending state change, give it a bit to clear */
		if (check_for_state_change(ftdmchan)) {
			SS7_ERROR("Failed to wait for pending state change on CIC = %d\n", sngss7_info->circuit->cic);
			ftdm_mutex_unlock(ftdmchan->mutex);
			SS7_ASSERT;
		};
		
		SS7_DEBUG_CHAN(ftdmchan, "Rx GRA (%d:%d)\n", 
								g_ftdm_sngss7_data.cfg.isupCkt[circuit].cic, 
								(g_ftdm_sngss7_data.cfg.isupCkt[circuit].cic + range));

		switch (ftdmchan->state) {
		/**********************************************************************/
		case FTDM_CHANNEL_STATE_RESTART:
			
			/* throw the FLAG_RESET_TX_RSP to indicate we have acknowledgement from the remote side */
			sngss7_set_flag(sngss7_info, FLAG_GRP_RESET_TX_RSP);

			/* go to DOWN */
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);

			break;
		/**********************************************************************/
		case FTDM_CHANNEL_STATE_DOWN:

			/* do nothing, just drop the message */
			SS7_DEBUG("Receveived GRA in down state, dropping\n");

			break;
		/**********************************************************************/
		case FTDM_CHANNEL_STATE_TERMINATING:
		case FTDM_CHANNEL_STATE_HANGUP:
		case FTDM_CHANNEL_STATE_HANGUP_COMPLETE:
			
			/* throw the FLAG_RESET_TX_RSP to indicate we have acknowledgement from the remote side */
			sngss7_set_flag(sngss7_info, FLAG_GRP_RESET_TX_RSP);

			break;
		/**********************************************************************/
		default:
			/* ITU Q764-2.9.5.1.c -> release the circuit */
			if ((siStaEvnt != NULL) &&
				(siStaEvnt->causeDgn.eh.pres ==PRSNT_NODEF) &&
				(siStaEvnt->causeDgn.causeVal.pres == PRSNT_NODEF)) {
				ftdmchan->caller_data.hangup_cause = siStaEvnt->causeDgn.causeVal.val;
			} else {
				ftdmchan->caller_data.hangup_cause = 98;	/* Message not compatiable with call state */
			}

			/* go to terminating to hang up the call */
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_TERMINATING);
			break;
		/**********************************************************************/
		}

		/* unlock the channel again before we exit */
		ftdm_mutex_unlock(ftdmchan->mutex);

	} /* for (( x = 0; x < (circuit + range); x++) */
	
	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_local_blk(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;

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
		SS7_ASSERT;
	};

	/* check if the circuit is already blocked or not */
	if (sngss7_test_flag(sngss7_info, FLAG_CKT_LC_BLOCK_RX)) {
		SS7_WARN("Received local BLO on circuit that is already blocked!\n");
	}

	/* throw the ckt block flag */
	sngss7_set_flag(sngss7_info, FLAG_CKT_LC_BLOCK_RX);

	/* set the channel to suspended state */
	ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);

	/* unlock the channel again before we exit */
	ftdm_mutex_unlock(ftdmchan->mutex);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_local_ubl(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;

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
		SS7_ASSERT;
	};

	/* check if the circuit is already blocked or not */
	if (sngss7_test_flag(sngss7_info, FLAG_CKT_LC_UNBLK_RX)) {
		SS7_WARN("Received local UBL on circuit that is already unblocked!\n");
	}

	/* throw the ckt block flag */
	sngss7_set_flag(sngss7_info, FLAG_CKT_LC_UNBLK_RX);

	/* set the channel to suspended state */
	ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);

	/* unlock the channel again before we exit */
	ftdm_mutex_unlock(ftdmchan->mutex);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t handle_ucic(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt)
{
	SS7_FUNC_TRACE_ENTER(__FUNCTION__);

	sngss7_chan_data_t  *sngss7_info = NULL;
	ftdm_channel_t	  *ftdmchan = NULL;

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
	   	SS7_ASSERT;
	};

	/* throw the ckt block flag */
	sngss7_set_flag(sngss7_info, FLAG_CKT_UCIC_BLOCK);

	/* set the channel to suspended state */
	ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_SUSPENDED);

	/* unlock the channel again before we exit */
	ftdm_mutex_unlock(ftdmchan->mutex);

	SS7_FUNC_TRACE_EXIT(__FUNCTION__);
	return FTDM_SUCCESS;
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
