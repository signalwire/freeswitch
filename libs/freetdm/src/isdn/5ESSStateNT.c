/*****************************************************************************

  FileName:     5ESSStateNT.c

  Contents:     AT&T 5ESS ISDN State Engine for NT (Network Mode).

	            The controlling state engine for Q.931 is the state engine
	            on the NT side. The state engine on the TE side is a slave
	            of this. The TE side maintain it's own states as described in
	            ITU-T Q931, but will in    raise conditions be overridden by
	            the NT side.

  License/Copyright:

  Copyright (c) 2007, Jan Vidar Berger, Case Labs, Ltd. All rights reserved.
  email:janvb@caselaboratories.com

  Copyright (c) 2007, Michael Jerris. All rights reserved.
  email:mike@jerris.com

  Copyright (c) 2007, Michael S. Collins, All rights reserved.
  email:mcollins@fcnetwork.com

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

#include "5ESS.h"

/*****************************************************************************
  Function:     ATT5ESSCreateNT

  Description:  Will create the AT&T 5ESS ISDN NT as a Dialect in the stack. The first
	            bulk set up the message handlers, the second bulk the IE
	            encoders/coders, and the last bulk set up the state table.

  Parameters:   i       Dialect index
*****************************************************************************/
void ATT5ESSCreateNT(L3UCHAR i)
{
	Q931SetMesProc(Q931mes_ALERTING,             i, Q931ProcAlertingNT,          Q931Umes_Alerting,          Q931Pmes_Alerting);
	Q931SetMesProc(Q931mes_CALL_PROCEEDING,      i, Q931ProcCallProceedingNT,    Q931Umes_CallProceeding,    Q931Pmes_CallProceeding);
	Q931SetMesProc(Q931mes_CONNECT,              i, Q931ProcConnectNT,           Q931Umes_Connect,           Q931Pmes_Connect);
	Q931SetMesProc(Q931mes_CONNECT_ACKNOWLEDGE,  i, Q931ProcConnectAckNT,        Q931Umes_ConnectAck,        Q931Pmes_ConnectAck);
	Q931SetMesProc(Q931mes_PROGRESS,             i, Q931ProcProgressNT,          Q931Umes_Progress,          Q931Pmes_Progress);
	Q931SetMesProc(Q931mes_SETUP,                i, Q931ProcSetupNT,             Q931Umes_Setup,             Q931Pmes_Setup);
	Q931SetMesProc(Q931mes_SETUP_ACKNOWLEDGE,    i, Q931ProcSetupAckNT,          Q931Umes_SetupAck,          Q931Pmes_SetupAck);
	Q931SetMesProc(Q931mes_RESUME,               i, Q931ProcResumeNT,            Q931Umes_Resume,            Q931Pmes_Resume);
	Q931SetMesProc(Q931mes_RESUME_ACKNOWLEDGE,   i, Q931ProcResumeAckNT,         Q931Umes_ResumeAck,         Q931Pmes_ResumeAck);
	Q931SetMesProc(Q931mes_RESUME_REJECT,        i, Q931ProcResumeRejectNT,      Q931Umes_ResumeReject,      Q931Pmes_ResumeReject);
	Q931SetMesProc(Q931mes_SUSPEND,              i, Q931ProcSuspendNT,           Q931Umes_Suspend,           Q931Pmes_Suspend);
	Q931SetMesProc(Q931mes_SUSPEND_ACKNOWLEDGE,  i, Q931ProcSuspendAckNT,        Q931Umes_SuspendAck,        Q931Pmes_SuspendAck);
	Q931SetMesProc(Q931mes_SUSPEND_REJECT,       i, Q931ProcSuspendRejectNT,     Q931Umes_SuspendReject,     Q931Pmes_SuspendReject);
	Q931SetMesProc(Q931mes_USER_INFORMATION,     i, Q931ProcUserInformationNT,   Q931Umes_UserInformation,   Q931Pmes_UserInformation);
	Q931SetMesProc(Q931mes_DISCONNECT,           i, Q931ProcDisconnectNT,        Q931Umes_Disconnect,        Q931Pmes_Disconnect);
	Q931SetMesProc(Q931mes_RELEASE,              i, Q931ProcReleaseNT,           Q931Umes_Release,           Q931Pmes_Release);
	Q931SetMesProc(Q931mes_RELEASE_COMPLETE,     i, Q931ProcReleaseCompleteNT,   Q931Umes_ReleaseComplete,   Q931Pmes_ReleaseComplete);
	Q931SetMesProc(Q931mes_RESTART,              i, Q931ProcRestartNT,           Q931Umes_Restart,           Q931Pmes_Restart);
	Q931SetMesProc(Q931mes_RESTART_ACKNOWLEDGE,  i, Q931ProcRestartAckNT,        Q931Umes_RestartAck,        Q931Pmes_RestartAck);
	Q931SetMesProc(Q931mes_CONGESTION_CONTROL,   i, Q931ProcCongestionControlNT, Q931Umes_CongestionControl, Q931Pmes_CongestionControl);
	Q931SetMesProc(Q931mes_INFORMATION,          i, Q931ProcInformationNT,       Q931Umes_Information,       Q931Pmes_Information);
	Q931SetMesProc(Q931mes_NOTIFY,               i, Q931ProcNotifyNT,            Q931Umes_Notify,            Q931Pmes_Notify);
	Q931SetMesProc(Q931mes_STATUS,               i, Q931ProcStatusNT,            Q931Umes_Status,            Q931Pmes_Status);
	Q931SetMesProc(Q931mes_STATUS_ENQUIRY,       i, Q931ProcStatusEnquiryNT,     Q931Umes_StatusEnquiry,     Q931Pmes_StatusEnquiry);
	Q931SetMesProc(Q931mes_SEGMENT,              i, Q931ProcSegmentNT,           Q931Umes_Segment,           Q931Pmes_Segment);

	Q931SetMesProc(Q932mes_FACILITY,             i, Q932ProcFacilityNT,          Q932Umes_Facility,          Q932Pmes_Facility);
	Q931SetMesProc(Q932mes_HOLD,                 i, Q932ProcHoldNT,              Q932Umes_Hold,              Q932Pmes_Hold);
	Q931SetMesProc(Q932mes_HOLD_ACKNOWLEDGE,     i, Q932ProcHoldAckNT,           Q932Umes_HoldAck,           Q932Pmes_HoldAck);
	Q931SetMesProc(Q932mes_HOLD_REJECT,          i, Q932ProcHoldRejectNT,        Q932Umes_HoldReject,        Q932Pmes_HoldReject);
	Q931SetMesProc(Q932mes_REGISTER,             i, Q932ProcRegisterNT,          Q932Umes_Register,          Q932Pmes_Register);
	Q931SetMesProc(Q932mes_RETRIEVE,             i, Q932ProcRetrieveNT,          Q932Umes_Retrieve,          Q932Pmes_Retrieve);
	Q931SetMesProc(Q932mes_RETRIEVE_ACKNOWLEDGE, i, Q932ProcRetrieveAckNT,       Q932Umes_RetrieveAck,       Q932Pmes_RetrieveAck);
	Q931SetMesProc(Q932mes_RETRIEVE_REJECT,      i, Q932ProcRetrieveRejectNT,    Q932Umes_RetrieveReject,    Q932Pmes_RetrieveReject);

	/* Set up the IE encoder/decoder handle table.*/
	Q931SetIEProc(Q931ie_SEGMENTED_MESSAGE,                i, Q931Pie_Segment,           Q931Uie_Segment);
	Q931SetIEProc(Q931ie_BEARER_CAPABILITY,                i, Q931Pie_BearerCap,         Q931Uie_BearerCap);
	Q931SetIEProc(Q931ie_CAUSE,                            i, Q931Pie_Cause,             Q931Uie_Cause);
	Q931SetIEProc(Q931ie_CALL_IDENTITY,                    i, Q931Pie_CallID,            Q931Uie_CallID);
	Q931SetIEProc(Q931ie_CALL_STATE,                       i, Q931Pie_CallState,         Q931Uie_CallState);
	Q931SetIEProc(Q931ie_CHANNEL_IDENTIFICATION,           i, Q931Pie_ChanID,            Q931Uie_ChanID);
	Q931SetIEProc(Q931ie_PROGRESS_INDICATOR,               i, Q931Pie_ProgInd,           Q931Uie_ProgInd);
	Q931SetIEProc(Q931ie_NETWORK_SPECIFIC_FACILITIES,      i, Q931Pie_NetFac,            Q931Uie_NetFac);
	Q931SetIEProc(Q931ie_NOTIFICATION_INDICATOR,           i, Q931Pie_NotifInd,          Q931Uie_NotifInd);
	Q931SetIEProc(Q931ie_DISPLAY,                          i, Q931Pie_Display,           Q931Uie_Display);
	Q931SetIEProc(Q931ie_DATETIME,                         i, Q931Pie_DateTime,          Q931Uie_DateTime);
	Q931SetIEProc(Q931ie_KEYPAD_FACILITY,                  i, Q931Pie_KeypadFac,         Q931Uie_KeypadFac);
	Q931SetIEProc(Q931ie_SIGNAL,                           i, Q931Pie_Signal,            Q931Uie_Signal);
	Q931SetIEProc(Q931ie_TRANSIT_DELAY_SELECTION_AND_IND,  i, Q931Pie_TransNetSel,       Q931Uie_TransNetSel);
	Q931SetIEProc(Q931ie_CALLING_PARTY_NUMBER,             i, Q931Pie_CallingNum,        Q931Uie_CallingNum);
	Q931SetIEProc(Q931ie_CALLING_PARTY_SUBADDRESS,         i, Q931Pie_CallingSub,        Q931Uie_CallingSub);
	Q931SetIEProc(Q931ie_CALLED_PARTY_NUMBER,              i, Q931Pie_CalledNum,         Q931Uie_CalledNum);
	Q931SetIEProc(Q931ie_CALLED_PARTY_SUBADDRESS,          i, Q931Pie_CalledSub,         Q931Uie_CalledSub);
	Q931SetIEProc(Q931ie_TRANSIT_NETWORK_SELECTION,        i, Q931Pie_TransNetSel,       Q931Uie_TransNetSel);
	Q931SetIEProc(Q931ie_RESTART_INDICATOR,                i, Q931Pie_RestartInd,        Q931Uie_RestartInd);
	Q931SetIEProc(Q931ie_LOW_LAYER_COMPATIBILITY,          i, Q931Pie_LLComp,            Q931Uie_LLComp);
	Q931SetIEProc(Q931ie_HIGH_LAYER_COMPATIBILITY,         i, Q931Pie_HLComp,            Q931Uie_HLComp);
	Q931SetIEProc(Q931ie_USER_USER,                        i, Q931Pie_UserUser,          Q931Uie_UserUser);
	Q931SetIEProc(Q931ie_GENERIC_DIGITS,                   i, Q931Pie_GenericDigits,     Q931Uie_GenericDigits);

	Q931SetIEProc(Q931ie_CONNECTED_NUMBER, i, Q931Pie_Generic, Q931Uie_Generic);
	Q931SetIEProc(Q931ie_FACILITY,         i, Q931Pie_Generic, Q931Uie_Generic);

	/* The following define a state machine. The point is that the Message  */
	/* procs can when search this to find out if the message/state          */
	/* combination is legale. If not, the proc for unexpected message apply.*/

	/* TODO define state table here */
}
