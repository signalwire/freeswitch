/*****************************************************************************

  FileName:		Q931api.c

  Contents:		api (Application Programming Interface) functions.
				See	q931.h for description. 

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
#include "memory.h"

/*
L3INT Q931CreateMesIndex(L3INT mc)
{
    if(mc < 0 || mc > 127 )
        return Q931E_INTERNAL;

    if(Q931MesCount >127)
        return Q931E_INTERNAL;

    Q931MesIndex[mc] = Q931MesCount ++;

    return Q931E_NO_ERROR;
}
*/
/*
L3INT Q931CreateIEIndex(L3INT iec)
{
    if(iec < 0 || iec > 127 )
        return Q931E_INTERNAL;

    if(Q931IECount > 127)
        return Q931E_INTERNAL;

    Q931IEIndex[iec] = Q931IECount ++;

    return Q931E_NO_ERROR;
}
*/

void Q931Api_InitTrunk(Q931_TrunkInfo *pTrunk)
{
	int y;
    pTrunk->LastCRV		= 0;
    pTrunk->Dialect		= 0;       
    pTrunk->Enabled		= 0;
    pTrunk->TrunkType	= Q931_TrType_E1;
    pTrunk->NetUser		= Q931_TE;
    pTrunk->TrunkState	= 0;
    for(y=0; y < Q931MAXCHPERTRUNK; y++)
    {
        pTrunk->ch[y].Available = 1;

        /* Set up E1 scheme by default */
        if(y==0)
        {
            pTrunk->ch[y].ChanType = Q931_ChType_Sync;
        }
        else if(y==16)
        {
            pTrunk->ch[y].ChanType = Q931_ChType_D;
        }
        else
        {
			pTrunk->ch[y].ChanType = Q931_ChType_B;
        }
    }

    for(y=0; y < Q931MAXCALLPERTRUNK; y++)
    {
        pTrunk->call[y].InUse = 0;

    }
}

void Q931SetMesProc(L3UCHAR mes, L3UCHAR dialect, 
                L3INT (*Q931ProcFunc)(Q931_TrunkInfo *pTrunk, L3UCHAR * b, L3INT iFrom),
                L3INT (*Q931UmesFunc)(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT IOff, L3INT Size),
                L3INT (*Q931PmesFunc)(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
)
{
    if(Q931ProcFunc != NULL)
        Q931Proc[dialect][mes] = Q931ProcFunc;
    if(Q931UmesFunc != NULL)
        Q931Umes[dialect][mes] = Q931UmesFunc;
    if(Q931PmesFunc != NULL)
        Q931Pmes[dialect][mes] = Q931PmesFunc;
}

void Q931SetIEProc(L3UCHAR iec, L3UCHAR dialect, 
			   L3INT (*Q931PieProc)(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet),
			   L3INT (*Q931UieProc)(Q931_TrunkInfo *pTrunk, ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff) 
)
{
    if(Q931PieProc != NULL)
        Q931Pie[dialect][iec] = Q931PieProc;
    if(Q931UieProc != NULL)
        Q931Uie[dialect][iec] = Q931UieProc;
}

#define trampoline(x) {x * t = (x *)pm; s = &t->buf[0];}

/*****************************************************************************

  Function:		Q931GetIEBuf

  Description:	Return a ptr to the buf used for IE in the message.

*****************************************************************************/
L3UCHAR * Q931GetIEBuf(L3UCHAR *pm)
{
	L3UCHAR * s=NULL;
	Q931mes_Alerting * pMes= (Q931mes_Alerting *)pm;
	switch(pMes->MesType)
	{
    case Q931mes_ALERTING             :
		trampoline(Q931mes_Alerting);
        break;

    case Q931mes_CALL_PROCEEDING      :
		trampoline(Q931mes_CallProceeding);
        break;

    case Q931mes_CONNECT              :      
		trampoline(Q931mes_Connect);
        break;

    case Q931mes_CONNECT_ACKNOWLEDGE  :
		trampoline(Q931mes_ConnectAck);
        break;

    case Q931mes_PROGRESS             :
		trampoline(Q931mes_Progress);
        break;

    case Q931mes_SETUP                :
		trampoline(Q931mes_Setup);
        break;

    case Q931mes_SETUP_ACKNOWLEDGE    :
		trampoline(Q931mes_SetupAck);
        break;

    case Q931mes_RESUME               :
		trampoline(Q931mes_Resume);
        break;

    case Q931mes_RESUME_ACKNOWLEDGE   :
		trampoline(Q931mes_ResumeAck);
        break;

    case Q931mes_RESUME_REJECT        :
		trampoline(Q931mes_ResumeReject);
        break;

    case Q932mes_RETRIEVE             :
		trampoline(Q932mes_Retrieve);
        break;

    case Q932mes_RETRIEVE_ACKNOWLEDGE :
		trampoline(Q932mes_RetrieveAck);
        break;

    case Q932mes_RETRIEVE_REJECT      :
		trampoline(Q932mes_RetrieveReject);
        break;

    case Q931mes_SUSPEND              :
		trampoline(Q931mes_Suspend);
        break;

    case Q931mes_SUSPEND_ACKNOWLEDGE  :
		trampoline(Q931mes_SuspendAck);
        break;

    case Q931mes_SUSPEND_REJECT       :
		trampoline(Q931mes_SuspendReject);
        break;

    case Q931mes_USER_INFORMATION     :
		trampoline(Q931mes_UserInformation);
        break;

    case Q931mes_DISCONNECT           :
		trampoline(Q931mes_Disconnect);
        break;

    case Q931mes_RELEASE              :
		trampoline(Q931mes_Release);
        break;

    case Q931mes_RELEASE_COMPLETE     :
		trampoline(Q931mes_ReleaseComplete);
        break;

    case Q931mes_RESTART              :
		trampoline(Q931mes_Restart);
        break;

    case Q931mes_RESTART_ACKNOWLEDGE  :
		trampoline(Q931mes_RestartAck);
        break;

    case Q931mes_CONGESTION_CONTROL   :
		trampoline(Q931mes_CongestionControl);
        break;

//    case Q931mes_FACILITY           :
//		trampoline(Q931mes_Facility);
//        break;

    case Q931mes_INFORMATION          :
		trampoline(Q931mes_Information);
        break;

    case Q931mes_NOTIFY               :
		trampoline(Q931mes_Notify);
        break;

//    case Q931mes_REGISTER           :
//		trampoline(Q931mes_Register);
//        break;

    case Q931mes_STATUS               :
		trampoline(Q931mes_Status);
        break;

    case Q931mes_STATUS_ENQUIRY       :
		trampoline(Q931mes_StatusEnquiry);
        break;

    case Q931mes_SEGMENT              :
		trampoline(Q931mes_Segment);
        break;

    default:
		s = 0;
        break;
    }

	return s;
}

L3INT Q931GetMesSize(L3UCHAR *pMes)
{
	
    L3UCHAR *p = Q931GetIEBuf(pMes);
    L3INT Size = (L3INT)(p - pMes);
    return Size;
}

/*****************************************************************************

  Function:     q931AppendIE    

  Description:  Append IE to the message.

  Parameters:   pm      Ptr to message.
                pi      Ptr to information element

  Return Value  ie setting

*****************************************************************************/

ie Q931AppendIE( L3UCHAR *pm, L3UCHAR *pi)
{
	ie IE = 0;
	Q931mes_Alerting * pMes= (Q931mes_Alerting *)pm;
	Q931ie_BearerCap * pIE= (Q931ie_BearerCap *)pi;
	L3INT iISize = pIE->Size;

	L3UCHAR *pBuf = Q931GetIEBuf(pm);
	L3INT Off = pMes->Size - (pBuf - pm);
	IE = Off | 0x8000;

	memcpy(&pm[pMes->Size], pi, iISize);

	pMes->Size += iISize;

	return IE;
}

/*****************************************************************************
*****************************************************************************/
L3INT Q931GetUniqueCRV(Q931_TrunkInfo *pTrunk)
{
	static L3INT crv={1};
	return crv++;
}

L3INT Q931InitMesSetup(Q931mes_Setup *pMes)
{
	pMes->ProtDisc		= 0x80;
	pMes->CRV			= 0;		/* CRV to be allocated, might be receive*/
	pMes->MesType		= Q931mes_SETUP;

	pMes->Size			= Q931GetMesSize((L3UCHAR*)pMes);

    pMes->SendComplete	=0;			/* Sending Complete                     */
    pMes->RepeatInd		=0;			/* Repeat Indicator                     */
    pMes->BearerCap		=0;			/* Bearer Capability                    */
    pMes->ChanID		=0;         /* Channel ID                           */
    pMes->ProgInd		=0;			/* Progress Indicator                   */
    pMes->NetFac		=0;         /* Network-specific facilities          */
    pMes->Display		=0;			/* Display                              */
    pMes->DateTime		=0;			/* Date/Time                            */
    pMes->KeypadFac		=0;			/* Keypad Facility                      */
    pMes->Signal		=0;         /* Signal                               */
    pMes->CallingNum	=0;			/* Calling party number                 */
    pMes->CallingSub	=0;			/* Calling party sub address            */
    pMes->CalledNum		=0;			/* Called party number                  */
    pMes->CalledSub		=0;			/* Called party sub address             */
    pMes->TransNetSel	=0;			/* Transit network selection            */
    pMes->LLRepeatInd	=0;			/* Repeat Indicator 2 LLComp            */
    pMes->LLComp		=0;         /* Low layer compatibility              */
    pMes->HLComp		=0;         /* High layer compatibility             */

	return 0;
}

L3INT Q931InitMesResume(Q931mes_Resume * pMes)
{
	pMes->ProtDisc		= 0x80;
	pMes->CRV			= 0;		/* CRV to be allocated, might be receive*/
	pMes->MesType		= Q931mes_RESUME;

	pMes->Size			= Q931GetMesSize((L3UCHAR*)pMes);
    pMes->CallID        = 0;        /* Channel Identification               */
	return 0;
}

L3INT Q931InitIEBearerCap(Q931ie_BearerCap *pIE)
{
	pIE->IEId			= Q931ie_BEARER_CAPABILITY;
	pIE->Size			= sizeof(Q931ie_BearerCap);
	pIE->CodStand		= 0;
	pIE->ITC			= 0;
	pIE->TransMode		= 0;
	pIE->ITR			= 0x10;
	pIE->RateMul		= 0;

	pIE->Layer1Ident	= 0;
	pIE->UIL1Prot		= 0;        /* User Information Layer 1 Protocol    */
	pIE->SyncAsync		= 0;        /* Sync/Async                           */
	pIE->Negot			= 0;                
	pIE->UserRate		= 0;
	pIE->InterRate		= 0;        /* Intermediate Rate                    */
	pIE->NIConTx		= 0;
	pIE->NIConRx		= 0;
	pIE->FlowCtlTx		= 0;        /* Flow control on Tx                   */
	pIE->FlowCtlRx		= 0;        /* Flow control on Rx                   */
	pIE->HDR			= 0;
	pIE->MultiFrame		= 0;        /* Multi frame support                  */
	pIE->Mode			= 0;
	pIE->LLInegot		= 0;
	pIE->Assignor		= 0;        /* Assignor/assignee                    */
	pIE->InBandNeg		= 0;        /* In-band/out-band negot.              */
	pIE->NumStopBits	= 0;        /* Number of stop bits                  */
	pIE->NumDataBits	= 0;        /* Number of data bits.                 */
	pIE->Parity			= 0;
	pIE->DuplexMode		= 0;
	pIE->ModemType		= 0;
	pIE->Layer2Ident	= 0;
	pIE->UIL2Prot		= 0;        /* User Information Layer 2 Protocol    */
	pIE->Layer3Ident	= 0;           
	pIE->UIL3Prot		= 0;        /* User Information Layer 3 Protocol    */
	pIE->AL3Info1		= 0;
	pIE->AL3Info2		= 0;

	return 0;
}

L3INT Q931InitIEChanID(Q931ie_ChanID *pIE)
{
	pIE->IEId			= Q931ie_CHANNEL_IDENTIFICATION;
	pIE->Size			= sizeof(Q931ie_ChanID);
	pIE->IntIDPresent	= 0;        /* Int. id. present                     */
	pIE->IntType		= 0;        /* Int. type                            */
	pIE->PrefExcl		= 0;        /* Pref./Excl.                          */
	pIE->DChanInd		= 0;        /* D-channel ind.                       */
	pIE->InfoChanSel	= 0;        /* Info. channel selection              */
	pIE->InterfaceID	= 0;        /* Interface identifier                 */
	pIE->CodStand		= 0;		/* Code standard                        */
	pIE->NumMap			= 0;        /* Number/Map                           */
	pIE->ChanMapType	= 0;        /* Channel type/Map element type        */
	pIE->ChanSlot		= 0;        /* Channel number/Slot map              */

	return 0;
}

L3INT Q931InitIEProgInd(Q931ie_ProgInd * pIE)
{
	pIE->IEId			= Q931ie_PROGRESS_INDICATOR;
	pIE->Size			= sizeof(Q931ie_ProgInd);
	pIE->CodStand		= 0;        /* Coding standard                      */
	pIE->Location		= 0;        /* Location                             */
	pIE->ProgDesc		= 0;        /* Progress description                 */

	return 0;
}

L3INT Q931InitIENetFac(Q931ie_NetFac * pIE)
{
	pIE->IEId			= Q931ie_NETWORK_SPECIFIC_FACILITIES;
	pIE->Size			= sizeof(Q931ie_NetFac);
	pIE->LenNetID		= 0;        /* Length of network facilities id.     */
	pIE->TypeNetID		= 0;        /* Type of network identification       */
	pIE->NetIDPlan		= 0;        /* Network identification plan.         */
	pIE->NetFac			= 0;        /* Network specific facility spec.      */
	pIE->NetID[0]		= 0;
	return 0;
}

L3INT Q931InitIEDisplay(Q931ie_Display * pIE)
{
	pIE->IEId			= Q931ie_DISPLAY;
	pIE->Size			= sizeof(Q931ie_Display);
	pIE->Display[0]		= 0;
	return 0;
}

L3INT Q931InitIEDateTime(Q931ie_DateTime * pIE)
{
	pIE->IEId			= Q931ie_DATETIME;
	pIE->Size			= sizeof(Q931ie_DateTime);
	pIE->Year			= 0;        /* Year                                 */
	pIE->Month			= 0;        /* Month                                */
	pIE->Day			= 0;        /* Day                                  */
	pIE->Hour			= 0;        /* Hour                                 */
	pIE->Minute			= 0;        /* Minute                               */
	pIE->Second			= 0;        /* Second                               */

	return 0;
}

L3INT Q931InitIEKeypadFac(Q931ie_KeypadFac * pIE)
{
	pIE->IEId			= Q931ie_KEYPAD_FACILITY;
	pIE->Size			= sizeof(Q931ie_KeypadFac);
	pIE->KeypadFac[0]	= 0;
	return 0;
}

L3INT Q931InitIESignal(Q931ie_Signal * pIE)
{
	pIE->IEId			= Q931ie_SIGNAL;
	pIE->Size			= sizeof(Q931ie_Signal);
	pIE->Signal			= 0;
	return 0;
}

L3INT Q931InitIECallingNum(Q931ie_CallingNum * pIE)
{
	pIE->IEId			= Q931ie_CALLING_PARTY_NUMBER;
	pIE->Size			= sizeof(Q931ie_CallingNum);
	pIE->TypNum			= 0;        /* Type of number                       */
	pIE->NumPlanID		= 0;        /* Numbering plan identification        */
	pIE->PresInd		= 0;        /* Presentation indicator               */
	pIE->ScreenInd		= 0;        /* Screening indicator                  */
	pIE->Digit[0]		= 0;        /* Number digits (IA5)                  */

	return 0;
}

L3INT Q931InitIECallingSub(Q931ie_CallingSub * pIE)
{
	pIE->IEId			= Q931ie_CALLING_PARTY_SUBADDRESS;
	pIE->Size			= sizeof(Q931ie_CallingSub);
	pIE->TypNum			= 0;        /* Type of subaddress                   */
	pIE->OddEvenInd		= 0;        /* Odd/Even indicator                   */
	pIE->Digit[0]		= 0;        /* Digits                               */

	return 0;
}

L3INT Q931InitIECalledNum(Q931ie_CalledNum * pIE)
{
	pIE->IEId			= Q931ie_CALLED_PARTY_NUMBER;
	pIE->Size			= sizeof(Q931ie_CalledNum);
	pIE->TypNum			= 0;        /* Type of Number                       */
	pIE->NumPlanID		= 0;        /* Numbering plan identification        */
	pIE->Digit[0]		= 0;        /* Digit (IA5)                          */

	return 0;
}

L3INT Q931InitIECalledSub(Q931ie_CalledSub * pIE)
{
	pIE->IEId			= Q931ie_CALLED_PARTY_SUBADDRESS;
	pIE->Size			= sizeof(Q931ie_CalledSub);
	pIE->TypNum			= 0;        /* Type of subaddress                   */
	pIE->OddEvenInd		= 0;        /* Odd/Even indicator                   */
	pIE->Digit[0]		= 0;        /* Digits                               */

	return 0;
}

L3INT Q931InitIETransNetSel(Q931ie_TransNetSel * pIE)
{
	pIE->IEId			= Q931ie_TRANSIT_NETWORK_SELECTION;
	pIE->Size			= sizeof(Q931ie_TransNetSel);
	pIE->Type			= 0;        /* Type of network identifier           */
	pIE->NetIDPlan		= 0;        /* Network idetification plan           */
	pIE->NetID[0]		= 0;        /* Network identification(IA5)          */

	return 0;
}

L3INT Q931InitIELLComp(Q931ie_LLComp * pIE)
{
	pIE->IEId			= Q931ie_LOW_LAYER_COMPATIBILITY;
	pIE->Size			= sizeof(Q931ie_LLComp);

	pIE->CodStand		= 0;        /* Coding standard                      */
	pIE->ITransCap		= 0;        /* Information transfer capability      */
	pIE->NegotInd		= 0;        /* Negot indic.                         */
	pIE->TransMode		= 0;        /* Transfer Mode                        */
	pIE->InfoRate		= 0;        /* Information transfer rate            */
	pIE->RateMul		= 0;        /* Rate multiplier                      */
	pIE->Layer1Ident	= 0;        /* Layer 1 ident.                       */
	pIE->UIL1Prot		= 0;        /* User information layer 1 protocol    */
	pIE->SyncAsync		= 0;        /* Synch/asynch                         */
	pIE->Negot			= 0;        /* Negot                                */
	pIE->UserRate		= 0;        /* User rate                            */
	pIE->InterRate		= 0;        /* Intermediate rate                    */
	pIE->NIConTx		= 0;        /* NIC on Tx                            */
	pIE->NIConRx		= 0;        /* NIC on Rx                            */
	pIE->FlowCtlTx		= 0;        /* Flow control on Tx                   */
	pIE->FlowCtlRx		= 0;        /* Flow control on Rx                   */
	pIE->HDR			= 0;        /* Hdr/no hdr                           */
	pIE->MultiFrame		= 0;        /* Multiframe                           */
	pIE->ModeL1			= 0;		/* Mode L1								*/
	pIE->NegotLLI		= 0;        /* Negot. LLI                           */
	pIE->Assignor		= 0;        /* Assignor/Assignor ee                 */
	pIE->InBandNeg		= 0;        /* In-band negot.                       */
	pIE->NumStopBits	= 0;        /* Number of stop bits                  */
	pIE->NumDataBits	= 0;        /* Number of data bits                  */
	pIE->Parity			= 0;        /* Parity                               */
	pIE->DuplexMode		= 0;        /* Duplex Mode                          */
	pIE->ModemType		= 0;        /* Modem type                           */
	pIE->Layer2Ident	= 0;        /* Layer 2 ident.                       */
	pIE->UIL2Prot		= 0;        /* User information layer 2 protocol    */
	pIE->ModeL2			= 0;        /* ModeL2                               */
	pIE->Q933use		= 0;        /* Q.9333 use                           */
	pIE->UsrSpcL2Prot	= 0;        /* User specified layer 2 protocol info */
	pIE->WindowSize		= 0;        /* Window size (k)                      */
	pIE->Layer3Ident	= 0;        /* Layer 3 ident                        */
	pIE->OptL3Info		= 0;        /* Optional layer 3 protocol info.      */
	pIE->ModeL3			= 0;        /* Mode of operation                    */
//	pIE->ModeX25op		= 0;        /* Mode of operation X.25               */
	pIE->DefPackSize	= 0;        /* Default packet size                  */
	pIE->PackWinSize	= 0;        /* Packet window size                   */
	pIE->AddL3Info		= 0;        /* Additional Layer 3 protocol info     */

	return 0;
}

L3INT Q931InitIEHLComp(Q931ie_HLComp * pIE)
{
	pIE->IEId			= Q931ie_HIGH_LAYER_COMPATIBILITY;
	pIE->Size			= sizeof(Q931ie_HLComp);

	return 0;
}

L3INT Q931ProcUnknownMessage(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom)
{
    return 0;
}

L3INT Q931ProcUnexpectedMessage(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom)
{
    return 0;
}

L3INT Q931Disconnect(Q931_TrunkInfo *pTrunk, L3INT iTo, L3INT iCRV, L3INT iCause)
{
    return 0;
}

L3INT Q931ReleaseComplete(Q931_TrunkInfo *pTrunk, L3INT iTo)
{
    return 0;
}
