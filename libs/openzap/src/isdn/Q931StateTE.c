/*****************************************************************************

  FileName:		q931StateTE.c

  Contents:		Q.931 State Engine for TE (User Mode).

			The controlling state engine for Q.931 is the state engine
			on the NT side. The state engine on the TE side is a slave 
			of this. The TE side maintain it's own states as described in
			ITU-T Q931, but will in	raise conditions be overridden by 
			the NT side.

			This reference implementation uses a process per message,
			meaning that each message must check call states. This
			is easier for dialect maintenance as each message proc
			can be replaced individually. A new TE variant only
			need to copy the Q931CreateTE and replace those procs or
			need to override.

  License/Copyright:

  Copyright (c) 2007, Jan Vidar Berger, Case Labs, Ltd. All rights reserved.
  email:janvb@caselaboratories.com  

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are 
  met:

	* Redistributions of source code must retain the above copyright notice, 
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice, 
	  this list of conditions and the following disclaimer in the documentation 
	  and/or other materials provided with the distribution.
	* Neither the name of the Case Labs, Ltd nor the names of its contributors 
	  may be used to endorse or promote products derived from this software 
	  without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
  POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include "Q931.h"

extern L3INT Q931L4HeaderSpace;

/*****************************************************************************
  Function:		Q931CreateTE

  Description:	Will create the Q931 TE as a Dialect in the stack. The first
				bulk set up the message handlers, the second bulk the IE
				encoders/coders, and the last bulk set up the state table.

  Parameters:	i		Dialect index
*****************************************************************************/
void Q931CreateTE(L3UCHAR i)
{
	Q931SetMesProc(Q931mes_ALERTING,             i, Q931ProcAlertingTE,          Q931Umes_Alerting,          Q931Pmes_Alerting);
	Q931SetMesProc(Q931mes_CALL_PROCEEDING,      i, Q931ProcCallProceedingTE,    Q931Umes_CallProceeding,    Q931Pmes_CallProceeding);
	Q931SetMesProc(Q931mes_CONNECT,              i, Q931ProcConnectTE,           Q931Umes_Connect,           Q931Pmes_Connect);
	Q931SetMesProc(Q931mes_CONNECT_ACKNOWLEDGE,  i, Q931ProcConnectAckTE,        Q931Umes_ConnectAck,        Q931Pmes_ConnectAck);
	Q931SetMesProc(Q931mes_PROGRESS,             i, Q931ProcProgressTE,          Q931Umes_Progress,          Q931Pmes_Progress);
	Q931SetMesProc(Q931mes_SETUP,                i, Q931ProcSetupTE,             Q931Umes_Setup,             Q931Pmes_Setup);
	Q931SetMesProc(Q931mes_SETUP_ACKNOWLEDGE,    i, Q931ProcSetupAckTE,          Q931Umes_SetupAck,          Q931Pmes_SetupAck);
	Q931SetMesProc(Q931mes_RESUME,               i, Q931ProcResumeTE,            Q931Umes_Resume,            Q931Pmes_Resume);
	Q931SetMesProc(Q931mes_RESUME_ACKNOWLEDGE,   i, Q931ProcResumeAckTE,         Q931Umes_ResumeAck,         Q931Pmes_ResumeAck);
	Q931SetMesProc(Q931mes_RESUME_REJECT,        i, Q931ProcResumeRejectTE,      Q931Umes_ResumeReject,      Q931Pmes_ResumeReject);
	Q931SetMesProc(Q931mes_SUSPEND,              i, Q931ProcSuspendTE,           Q931Umes_Suspend,           Q931Pmes_Suspend);
	Q931SetMesProc(Q931mes_SUSPEND_ACKNOWLEDGE,  i, Q931ProcSuspendAckTE,        Q931Umes_SuspendAck,        Q931Pmes_SuspendAck);
	Q931SetMesProc(Q931mes_SUSPEND_REJECT,       i, Q931ProcSuspendRejectTE,     Q931Umes_SuspendReject,     Q931Pmes_SuspendReject);
	Q931SetMesProc(Q931mes_USER_INFORMATION,     i, Q931ProcUserInformationTE,   Q931Umes_UserInformation,   Q931Pmes_UserInformation);
	Q931SetMesProc(Q931mes_DISCONNECT,           i, Q931ProcDisconnectTE,        Q931Umes_Disconnect,        Q931Pmes_Disconnect);
	Q931SetMesProc(Q931mes_RELEASE,              i, Q931ProcReleaseTE,           Q931Umes_Release,           Q931Pmes_Release);
	Q931SetMesProc(Q931mes_RELEASE_COMPLETE,     i, Q931ProcReleaseCompleteTE,   Q931Umes_ReleaseComplete,   Q931Pmes_ReleaseComplete);
	Q931SetMesProc(Q931mes_RESTART,              i, Q931ProcRestartTE,           Q931Umes_Restart,           Q931Pmes_Restart);
	Q931SetMesProc(Q931mes_RESTART_ACKNOWLEDGE,  i, Q931ProcRestartAckTE,        Q931Umes_RestartAck,        Q931Pmes_RestartAck);
	Q931SetMesProc(Q931mes_CONGESTION_CONTROL,   i, Q931ProcCongestionControlTE, Q931Umes_CongestionControl, Q931Pmes_CongestionControl);
	Q931SetMesProc(Q931mes_INFORMATION,          i, Q931ProcInformationTE,       Q931Umes_Information,       Q931Pmes_Information);
	Q931SetMesProc(Q931mes_NOTIFY,               i, Q931ProcNotifyTE,            Q931Umes_Notify,            Q931Pmes_Notify);
	Q931SetMesProc(Q931mes_STATUS,               i, Q931ProcStatusTE,            Q931Umes_Status,            Q931Pmes_Status);
	Q931SetMesProc(Q931mes_STATUS_ENQUIRY,       i, Q931ProcStatusEnquiryTE,     Q931Umes_StatusEnquiry,     Q931Pmes_StatusEnquiry);
	Q931SetMesProc(Q931mes_SEGMENT,              i, Q931ProcSegmentTE,           Q931Umes_Segment,           Q931Pmes_Segment);

	Q931SetMesProc(Q932mes_FACILITY,             i, Q932ProcFacilityTE,          Q932Umes_Facility,          Q932Pmes_Facility);
	Q931SetMesProc(Q932mes_HOLD,                 i, Q932ProcHoldTE,              Q932Umes_Hold,              Q932Pmes_Hold);
	Q931SetMesProc(Q932mes_HOLD_ACKNOWLEDGE,     i, Q932ProcHoldAckTE,           Q932Umes_HoldAck,           Q932Pmes_HoldAck);
	Q931SetMesProc(Q932mes_HOLD_REJECT,          i, Q932ProcHoldRejectTE,        Q932Umes_HoldReject,        Q932Pmes_HoldReject);
	Q931SetMesProc(Q932mes_REGISTER,             i, Q932ProcRegisterTE,          Q932Umes_Register,          Q932Pmes_Register);
	Q931SetMesProc(Q932mes_RETRIEVE,             i, Q932ProcRetrieveTE,          Q932Umes_Retrieve,          Q932Pmes_Retrieve);
	Q931SetMesProc(Q932mes_RETRIEVE_ACKNOWLEDGE, i, Q932ProcRetrieveAckTE,       Q932Umes_RetrieveAck,       Q932Pmes_RetrieveAck);
	Q931SetMesProc(Q932mes_RETRIEVE_REJECT,      i, Q932ProcRetrieveRejectTE,    Q932Umes_RetrieveReject,    Q932Pmes_RetrieveReject);

	/* Set up the IE encoder/decoder handle table.*/ 
	Q931SetIEProc(Q931ie_SEGMENTED_MESSAGE,                i, Q931Pie_Segment,     Q931Uie_Segment);
	Q931SetIEProc(Q931ie_BEARER_CAPABILITY,                i, Q931Pie_BearerCap,   Q931Uie_BearerCap);
	Q931SetIEProc(Q931ie_CAUSE,                            i, Q931Pie_Cause,       Q931Uie_Cause);
	Q931SetIEProc(Q931ie_CALL_IDENTITY,                    i, Q931Pie_CallID,      Q931Uie_CallID);
	Q931SetIEProc(Q931ie_CALL_STATE,                       i, Q931Pie_CallState,   Q931Uie_CallState);
	Q931SetIEProc(Q931ie_CHANNEL_IDENTIFICATION,           i, Q931Pie_ChanID,      Q931Uie_ChanID);
	Q931SetIEProc(Q931ie_PROGRESS_INDICATOR,               i, Q931Pie_ProgInd,     Q931Uie_ProgInd);
	Q931SetIEProc(Q931ie_NETWORK_SPECIFIC_FACILITIES,      i, Q931Pie_NetFac,      Q931Uie_NetFac);
	Q931SetIEProc(Q931ie_NOTIFICATION_INDICATOR,           i, Q931Pie_NotifInd,    Q931Uie_NotifInd);
	Q931SetIEProc(Q931ie_DISPLAY,                          i, Q931Pie_Display,     Q931Uie_Display);
	Q931SetIEProc(Q931ie_DATETIME,                         i, Q931Pie_DateTime,    Q931Uie_DateTime);
	Q931SetIEProc(Q931ie_KEYPAD_FACILITY,                  i, Q931Pie_KeypadFac,   Q931Uie_KeypadFac);
	Q931SetIEProc(Q931ie_SIGNAL,                           i, Q931Pie_Signal,      Q931Uie_Signal);
	Q931SetIEProc(Q931ie_TRANSIT_DELAY_SELECTION_AND_IND,  i, Q931Pie_TransNetSel, Q931Uie_TransNetSel);
	Q931SetIEProc(Q931ie_CALLING_PARTY_NUMBER,             i, Q931Pie_CallingNum,  Q931Uie_CallingNum);
	Q931SetIEProc(Q931ie_CALLING_PARTY_SUBADDRESS,         i, Q931Pie_CallingSub,  Q931Uie_CallingSub);
	Q931SetIEProc(Q931ie_CALLED_PARTY_NUMBER,              i, Q931Pie_CalledNum,   Q931Uie_CalledNum);
	Q931SetIEProc(Q931ie_CALLED_PARTY_SUBADDRESS,          i, Q931Pie_CalledSub,   Q931Uie_CalledSub);
	Q931SetIEProc(Q931ie_TRANSIT_NETWORK_SELECTION,        i, Q931Pie_TransNetSel, Q931Uie_TransNetSel);
	Q931SetIEProc(Q931ie_RESTART_INDICATOR,                i, Q931Pie_RestartInd,  Q931Uie_RestartInd);
	Q931SetIEProc(Q931ie_LOW_LAYER_COMPATIBILITY,          i, Q931Pie_LLComp,      Q931Uie_LLComp);
	Q931SetIEProc(Q931ie_HIGH_LAYER_COMPATIBILITY,         i, Q931Pie_HLComp,      Q931Uie_HLComp);
	Q931SetIEProc(Q931ie_USER_USER,                        i, Q931Pie_UserUser,    Q931Uie_UserUser);

	Q931SetIEProc(Q931ie_CONNECTED_NUMBER,   i, Q931Pie_Generic, Q931Uie_Generic);
	Q931SetIEProc(Q931ie_FACILITY,           i, Q931Pie_Generic, Q931Uie_Generic);
	Q931SetIEProc(Q931ie_REDIRECTING_NUMBER, i, Q931Pie_Generic, Q931Uie_Generic);

	/* The following define a state machine. The point is that the Message
	 * procs can when search this to find out if the message/state
	 * combination is legale. If not, the proc for unexpected message apply.
	 */

	/* State 0 Idle */
	Q931AddStateEntry(i, Q931_U0, Q931mes_RESUME,           2);
	Q931AddStateEntry(i, Q931_U0, Q931mes_SETUP,            4);
	Q931AddStateEntry(i, Q931_U0, Q931mes_SETUP,            2);
	Q931AddStateEntry(i, Q931_U0, Q931mes_STATUS,           4);
	Q931AddStateEntry(i, Q931_U0, Q931mes_RELEASE,          4);
	Q931AddStateEntry(i, Q931_U0, Q931mes_RELEASE_COMPLETE, 4);

	/* State 1 Call Initiating */
	Q931AddStateEntry(i, Q931_U1, Q931mes_DISCONNECT,        2);
	Q931AddStateEntry(i, Q931_U1, Q931mes_SETUP_ACKNOWLEDGE, 4);
	Q931AddStateEntry(i, Q931_U1, Q931mes_RELEASE_COMPLETE,  4);
	Q931AddStateEntry(i, Q931_U1, Q931mes_CALL_PROCEEDING,   4);
	Q931AddStateEntry(i, Q931_U1, Q931mes_ALERTING,          4);
	Q931AddStateEntry(i, Q931_U1, Q931mes_CONNECT,           4);

	/* State 2 Overlap Sending */
	Q931AddStateEntry(i, Q931_U2, Q931mes_INFORMATION,     2);
	Q931AddStateEntry(i, Q931_U2, Q931mes_CALL_PROCEEDING, 4);
	Q931AddStateEntry(i, Q931_U2, Q931mes_ALERTING,        4);
	Q931AddStateEntry(i, Q931_U2, Q931mes_PROGRESS,        4);
	Q931AddStateEntry(i, Q931_U2, Q931mes_CONNECT,         4);
	Q931AddStateEntry(i, Q931_U2, Q931mes_RELEASE,         2);

	/* State 3 Outgoing Call Proceeding */
	Q931AddStateEntry(i, Q931_U3, Q931mes_PROGRESS, 4);
	Q931AddStateEntry(i, Q931_U3, Q931mes_ALERTING, 4);
	Q931AddStateEntry(i, Q931_U3, Q931mes_CONNECT,  4);
	Q931AddStateEntry(i, Q931_U3, Q931mes_RELEASE,  2);

	/* State 4 Call Delivered */
	Q931AddStateEntry(i, Q931_U4, Q931mes_CONNECT, 4);

	/* State 6 Call Precent */
	Q931AddStateEntry(i, Q931_U6, Q931mes_INFORMATION,      2);
	Q931AddStateEntry(i, Q931_U6, Q931mes_ALERTING,         2);
	Q931AddStateEntry(i, Q931_U6, Q931mes_CALL_PROCEEDING,  2);
	Q931AddStateEntry(i, Q931_U6, Q931mes_CONNECT,          2);
	Q931AddStateEntry(i, Q931_U6, Q931mes_RELEASE_COMPLETE, 2);
	Q931AddStateEntry(i, Q931_U6, Q931mes_RELEASE,          4);
	Q931AddStateEntry(i, Q931_U6, Q931mes_DISCONNECT,       4);

	/* State 7 Call Received */
	Q931AddStateEntry(i, Q931_U7, Q931mes_CONNECT, 2);

	/* State 8 Connect request */
	Q931AddStateEntry(i, Q931_U8, Q931mes_CONNECT_ACKNOWLEDGE, 4);

	/* State 9 Incoming Call Proceeding */
	Q931AddStateEntry(i, Q931_U9, Q931mes_CONNECT,  2);
	Q931AddStateEntry(i, Q931_U9, Q931mes_ALERTING, 2);
	Q931AddStateEntry(i, Q931_U9, Q931mes_PROGRESS, 2);

	/* State 10 Active */
	Q931AddStateEntry(i, Q931_U10, Q931mes_SUSPEND, 2);
	Q931AddStateEntry(i, Q931_U10, Q931mes_NOTIFY,  4);
	Q931AddStateEntry(i, Q931_U10, Q931mes_NOTIFY,  2);

	/* State 11 Disconnect Request */
	Q931AddStateEntry(i, Q931_U11, Q931mes_RELEASE,    4);
	Q931AddStateEntry(i, Q931_U11, Q931mes_DISCONNECT, 4);
	Q931AddStateEntry(i, Q931_U11, Q931mes_NOTIFY,     4);

	/* State 12 Disconnect Ind */
	Q931AddStateEntry(i, Q931_U12, Q931mes_RELEASE, 4);
	Q931AddStateEntry(i, Q931_U12, Q931mes_RELEASE, 2);

	/* State 15 Suspend Request */
	Q931AddStateEntry(i, Q931_U15, Q931mes_SUSPEND_ACKNOWLEDGE, 4);
	Q931AddStateEntry(i, Q931_U15, Q931mes_SUSPEND_REJECT,      4);
	Q931AddStateEntry(i, Q931_U15, Q931mes_DISCONNECT,          4);
	Q931AddStateEntry(i, Q931_U15, Q931mes_RELEASE,             4);

/* TODO
	Q931AddStateEntry(i, Q931_U17,
	Q931AddStateEntry(i, Q931_U19,
	Q931AddStateEntry(i, Q931_U25,
*/

	/* Timer default values */
	Q931SetTimerDefault(i, Q931_TIMER_T301, 180000);	/* T301: 180s */
	Q931SetTimerDefault(i, Q931_TIMER_T302,  15000);	/* T302:  15s */
	Q931SetTimerDefault(i, Q931_TIMER_T303,   4000);	/* T303:   4s */
	Q931SetTimerDefault(i, Q931_TIMER_T304,  30000);	/* T304:  30s */
	Q931SetTimerDefault(i, Q931_TIMER_T305,  30000);	/* T305:  30s */
	Q931SetTimerDefault(i, Q931_TIMER_T308,   4000);	/* T308:   4s */
	Q931SetTimerDefault(i, Q931_TIMER_T309,  60000);	/* T309:  60s */
	Q931SetTimerDefault(i, Q931_TIMER_T310,  60000);	/* T310:  60s */
	Q931SetTimerDefault(i, Q931_TIMER_T313,   4000);	/* T313:   4s */
	Q931SetTimerDefault(i, Q931_TIMER_T314,   4000);	/* T314:   4s */
	Q931SetTimerDefault(i, Q931_TIMER_T316, 120000);	/* T316: 120s */
	Q931SetTimerDefault(i, Q931_TIMER_T317,  90000);	/* T317:  90s */
	Q931SetTimerDefault(i, Q931_TIMER_T318,   4000);	/* T318:   4s */
	Q931SetTimerDefault(i, Q931_TIMER_T319,   4000);	/* T319:   4s */
	Q931SetTimerDefault(i, Q931_TIMER_T321,  30000);	/* T321:  30s */
	Q931SetTimerDefault(i, Q931_TIMER_T322,   4000);	/* T322:   4s */
}

/*****************************************************************************

  Function:		Q931ProcAlertingTE

*****************************************************************************/
L3INT Q931ProcAlertingTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* Reset 4 sec timer. */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom == 4) {
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcCallProceedingTE

*****************************************************************************/
L3INT Q931ProcCallProceedingTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom == 4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcConnectTE

*****************************************************************************/
L3INT Q931ProcConnectTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
		if (pTrunk->autoConnectAck) {
			Q931AckConnect(pTrunk, buf);
		}
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcConnectAckTE

*****************************************************************************/
L3INT Q931ProcConnectAckTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcProgressTE

*****************************************************************************/
L3INT Q931ProcProgressTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcSetupTE

*****************************************************************************/
L3INT Q931ProcSetupTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)&buf[Q931L4HeaderSpace];
	L3INT rc = 0;
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Reject SETUP on existing calls */
	if (Q931GetCallState(pTrunk, pMes->CRV) != Q931_U0) {
		Q931Disconnect(pTrunk, iFrom, pMes->CRV, 81);
		return Q931E_UNEXPECTED_MESSAGE;
	}

	/* outgoing call */
	if (iFrom == 4) {
		ret = Q931CreateCRV(pTrunk, &callIndex);
		if (ret)
			return ret;
		pMes->CRV = pTrunk->call[callIndex].CRV;

		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
		if (ret)
			return ret;

		Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);

		/* TODO: Add this back when we get the state stuff more filled out */
		/*Q931SetState(pTrunk, callIndex, Q931_U1);*/
	}
	/* incoming call */
	else {
		/* Locate free CRV entry and store info */
		ret = Q931AllocateCRV(pTrunk, pMes->CRV, &callIndex);
		if (ret != Q931E_NO_ERROR) {
			/* Not possible to allocate CRV entry, so must reject call */
			Q931Disconnect(pTrunk, iFrom, pMes->CRV, 42);
			return ret;
		}

		/* Send setup indication to user */
		ret = Q931Tx34(pTrunk, (L3UCHAR*)pMes, pMes->Size);
		if (ret != Q931E_NO_ERROR) {
			if (pTrunk->autoSetupAck) {
				Q931AckSetup(pTrunk, buf);
			}
			return ret;
		} else {
			/* Must be full queue, meaning we can't process the call */
			/* so we must disconnect */
			Q931Disconnect(pTrunk, iFrom, pMes->CRV, 81);
			 return ret;
		}
#if 0
		/* TODO: Unreachable code??? */
		/* Set state U6 */
		Q931SetState(pTrunk, callIndex, Q931_U6);

		Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
#endif
	}
	return rc;
}

/*****************************************************************************

  Function:		Q931ProcSetupAckTE

  Description:	Used to acknowedge a SETUP. Usually the first initial
				response recevide back used to buy some time. L4 sending this
				should only be passed on. L2 sending this means that we set
				a new timer (and pass it to L4).

				Note that ChanID (B Channel Assignment) might come here from
				NT side.

*****************************************************************************/
L3INT Q931ProcSetupAckTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcResumeTE

*****************************************************************************/
L3INT Q931ProcResumeTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Generic * pMes = (Q931mes_Generic *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	if (Q931GetCallState(pTrunk, pMes->CRV) == Q931_U0 && iFrom ==4) {
		/* Call reference selection */
		ret = Q931CreateCRV(pTrunk, &callIndex);
		if (ret != Q931E_NO_ERROR)
			return ret;
		pMes->CRV = pTrunk->call[callIndex].CRV;

		/* Send RESUME to network */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
		if (ret != Q931E_NO_ERROR)
			return ret;

		/* Start timer T318 */
		Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T318);

		/* set state U17 */
		Q931SetState(pTrunk, callIndex, Q931_U17);
	} else {
		return Q931E_ILLEGAL_MESSAGE;
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcResumeAckTE

*****************************************************************************/
L3INT Q931ProcResumeAckTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcResumeRejectTE

*****************************************************************************/
L3INT Q931ProcResumeRejectTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcSuspendTE

*****************************************************************************/
L3INT Q931ProcSuspendTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcSuspendAckTE

*****************************************************************************/
L3INT Q931ProcSuspendAckTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcSuspendRejectTE

*****************************************************************************/
L3INT Q931ProcSuspendRejectTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcInformationTE

*****************************************************************************/
L3INT Q931ProcUserInformationTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcDisconnectTE

*****************************************************************************/
L3INT Q931ProcDisconnectTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	Q931Log(pTrunk, Q931_LOG_DEBUG, "Processing DISCONNECT message from %s for CRV: %d (%#hx)\n",
						 iFrom == 4 ? "Local" : "Remote", pMes->CRV, pMes->CRV);

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcReleaseTE

*****************************************************************************/
L3INT Q931ProcReleaseTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT state = Q931GetCallState(pTrunk, pMes->CRV);
	L3INT ret = Q931E_NO_ERROR;

	if (iFrom == 4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (state == Q931_U0 && iFrom == 2) {
		Q931Tx34(pTrunk, buf, pMes->Size);
		ret = Q931ReleaseComplete(pTrunk, buf);
	} else {
		ret = Q931ProcUnexpectedMessage(pTrunk, buf, iFrom);
	}
	if (pMes->CRV && iFrom == 2) {
		/* Find the call using CRV */
		if ((Q931FindCRV(pTrunk, pMes->CRV, &callIndex)) != Q931E_NO_ERROR)
			return ret;
		pTrunk->call[callIndex].InUse = 0;
	}

	return ret;
}

/*****************************************************************************

  Function:		Q931ProcReleaseCompleteTE

*****************************************************************************/
L3INT Q931ProcReleaseCompleteTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	} else {
		if (pMes->CRV) {
			/* Find the call using CRV */
			ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
			if (ret != Q931E_NO_ERROR)
				return ret;
			pTrunk->call[callIndex].InUse = 0;

			/* TODO: experimental, send RELEASE_COMPLETE message */
		        ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
		}
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcRestartTE

*****************************************************************************/
L3INT Q931ProcRestartTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	if (pMes->CRV) {
		/* Find the call using CRV */
		ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
		if (ret != Q931E_NO_ERROR)
			return ret;

		/* TODO - Set correct timer here */
		Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	}

	/* TODO chack against state table for illegal or unexpected message here */

	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);

		if (pTrunk->autoRestartAck) {
			Q931AckRestart(pTrunk, buf);
		}
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcRestartAckTE

*****************************************************************************/
L3INT Q931ProcRestartAckTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	if (pMes->CRV) {
		/* Find the call using CRV */
		ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
		if (ret != Q931E_NO_ERROR)
			return ret;
		/* TODO - Set correct timer here */
		Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	}

	/* TODO chack against state table for illegal or unexpected message here */

	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcCongestionControlTE

*****************************************************************************/
L3INT Q931ProcCongestionControlTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcInformationTE

*****************************************************************************/
L3INT Q931ProcInformationTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcNotifyTE

*****************************************************************************/
L3INT Q931ProcNotifyTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcStatusTE

*****************************************************************************/
L3INT Q931ProcStatusTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcStatusEnquiryTE

*****************************************************************************/
L3INT Q931ProcStatusEnquiryTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcSegmentTE

*****************************************************************************/
L3INT Q931ProcSegmentTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/****************************************************************************/
/******************* Q.932 - Supplementary Services *************************/
/****************************************************************************/

/*****************************************************************************

  Function:		Q932ProcRetrieveTE

*****************************************************************************/
L3INT Q932ProcFacilityTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q932ProcRetrieveTE

*****************************************************************************/
L3INT Q932ProcHoldTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q932ProcRetrieveTE

*****************************************************************************/
L3INT Q932ProcHoldAckTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q932ProcRetrieveTE

*****************************************************************************/
L3INT Q932ProcHoldRejectTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q932ProcRegisterTE

*****************************************************************************/
L3INT Q932ProcRegisterTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q932ProcRetrieveTE

*****************************************************************************/
L3INT Q932ProcRetrieveTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcRetrieveAckTE

*****************************************************************************/
L3INT Q932ProcRetrieveAckTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}

/*****************************************************************************

  Function:		Q931ProcRetrieveRejectTE

*****************************************************************************/
L3INT Q932ProcRetrieveRejectTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom)
{
	Q931mes_Header *pMes = (Q931mes_Header *)&buf[Q931L4HeaderSpace];
	L3INT callIndex;
	L3INT ret = Q931E_NO_ERROR;

	/* Find the call using CRV */
	ret = Q931FindCRV(pTrunk, pMes->CRV, &callIndex);
	if (ret != Q931E_NO_ERROR)
		return ret;

	/* TODO chack against state table for illegal or unexpected message here */

	/* TODO - Set correct timer here */
	Q931StartTimer(pTrunk, callIndex, Q931_TIMER_T303);
	if (iFrom ==4) {
		/* TODO Add proc here */
		ret = Q931Tx32Data(pTrunk, 0, buf, pMes->Size);
	}
	else if (iFrom == 2) {
		/* TODO Add proc here */
		ret = Q931Tx34(pTrunk, buf, pMes->Size);
	}
	return ret;
}
