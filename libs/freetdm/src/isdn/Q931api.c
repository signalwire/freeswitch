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

extern L3INT Q931L4HeaderSpace;

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

L3INT Q931Api_InitTrunk(Q931_TrunkInfo_t *pTrunk,
						Q931Dialect_t Dialect,
						Q931NetUser_t NetUser,
						Q931_TrunkType_t TrunkType,
						Q931Tx34CB_t Q931Tx34CBProc,
						Q931Tx32CB_t Q931Tx32CBProc,
						Q931ErrorCB_t Q931ErrorCBProc,
						void *PrivateData32,
						void *PrivateData34)
{
	int y, dchannel, maxchans, has_sync = 0;

	switch(TrunkType)
	{
	case Q931_TrType_E1:
		dchannel = 16;
		maxchans = 31;
		has_sync = 1;
		break;

	case Q931_TrType_T1:
	case Q931_TrType_J1:
		dchannel = 24;
		maxchans = 24;
		break;

	case Q931_TrType_BRI:
	case Q931_TrType_BRI_PTMP:
		dchannel = 3;
		maxchans = 3;
		break;

	default:
		return 0;
	}

	pTrunk->Q931Tx34CBProc = Q931Tx34CBProc;
	pTrunk->Q931Tx32CBProc = Q931Tx32CBProc;
	pTrunk->Q931ErrorCBProc = Q931ErrorCBProc;
	pTrunk->PrivateData32 = PrivateData32;
	pTrunk->PrivateData34 = PrivateData34;

    pTrunk->LastCRV			= 0;
    pTrunk->Dialect			= Dialect + NetUser;       
    pTrunk->Enabled			= 0;
    pTrunk->TrunkType		= TrunkType;
    pTrunk->NetUser			= NetUser;
    pTrunk->TrunkState		= 0;
	pTrunk->autoRestartAck	= 0;
    for(y=0; y < Q931MAXCHPERTRUNK; y++)
    {
        pTrunk->ch[y].Available = 1;

        if(has_sync && y == 0)
        {
            pTrunk->ch[y].ChanType = Q931_ChType_Sync;
        }
        else if(y == dchannel)
        {
            pTrunk->ch[y].ChanType = Q931_ChType_D;
        }
        else if(y > maxchans)
        {
            pTrunk->ch[y].ChanType = Q931_ChType_NotUsed;
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
	return 1;
}

void Q931SetMesProc(L3UCHAR mes, L3UCHAR dialect, q931proc_func_t *Q931ProcFunc, q931umes_func_t *Q931UmesFunc, q931pmes_func_t *Q931PmesFunc)
{
    if(Q931ProcFunc != NULL)
        Q931Proc[dialect][mes] = Q931ProcFunc;
    if(Q931UmesFunc != NULL)
        Q931Umes[dialect][mes] = Q931UmesFunc;
    if(Q931PmesFunc != NULL)
        Q931Pmes[dialect][mes] = Q931PmesFunc;
}

void Q931SetIEProc(L3UCHAR iec, L3UCHAR dialect, q931pie_func_t *Q931PieProc, q931uie_func_t *Q931UieProc)
{
    if(Q931PieProc != NULL)
        Q931Pie[dialect][iec] = Q931PieProc;
    if(Q931UieProc != NULL)
        Q931Uie[dialect][iec] = Q931UieProc;
}

void Q931SetTimeoutProc(L3UCHAR dialect, L3UCHAR timer, q931timeout_func_t *Q931TimeoutProc)
{
	if(Q931Timeout != NULL)
		Q931Timeout[dialect][timer] = Q931TimeoutProc;
}

void Q931SetTimerDefault(L3UCHAR dialect, L3UCHAR timer, q931timer_t timeout)
{
	Q931Timer[dialect][timer] = timeout;
}

L3INT Q931GetMesSize(Q931mes_Generic *pMes)
{
	
    L3UCHAR *p = &pMes->buf[0];
    L3INT Size = (L3INT)(p - (L3UCHAR *)pMes);
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
	Q931mes_Generic * pMes= (Q931mes_Generic *)pm;
	Q931ie_BearerCap * pIE= (Q931ie_BearerCap *)pi;
	L3INT iISize = pIE->Size;

	L3UCHAR *pBuf = &pMes->buf[0];
	L3INT Off = (L3INT)(pMes->Size - (pBuf - pm));
	IE = (ie)(Off | 0x8000);

	memcpy(&pm[pMes->Size], pi, iISize);

	pMes->Size += iISize;

	return IE;
}

/*****************************************************************************
*****************************************************************************/
static L3INT crv={1};

L3INT Q931GetUniqueCRV(Q931_TrunkInfo_t *pTrunk)
{
	L3INT max = (Q931_IS_BRI(pTrunk)) ? Q931_BRI_MAX_CRV : Q931_PRI_MAX_CRV;

	crv++;
	crv = (crv <= max) ? crv : 1;

	return crv;
}

L3INT Q931InitMesGeneric(Q931mes_Generic *pMes)
{
	memset(pMes, 0, sizeof(*pMes));
	pMes->ProtDisc		= 0x08;
	pMes->Size			= Q931GetMesSize(pMes);

	return 0;
}

L3INT Q931InitMesResume(Q931mes_Generic * pMes)
{
	pMes->ProtDisc		= 0x08;
	pMes->CRV			= 0;		/* CRV to be allocated, might be receive*/
	pMes->MesType		= Q931mes_RESUME;

	pMes->Size			= Q931GetMesSize(pMes);
    pMes->CallID        = 0;        /* Channel Identification               */
	return 0;
}

L3INT Q931InitMesRestartAck(Q931mes_Generic * pMes)
{
	pMes->ProtDisc		= 0x08;
	pMes->CRV			= 0;		/* CRV to be allocated, might be receive*/
	pMes->MesType		= Q931mes_RESTART_ACKNOWLEDGE;

	pMes->Size			= Q931GetMesSize(pMes);
    pMes->ChanID        = 0;        /* Channel Identification               */
	pMes->Display		= 0;
	pMes->RestartInd	= 0;
	pMes->RestartWin	= 0;
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
#if 0
	pIE->ModeX25op		= 0;        /* Mode of operation X.25               */
#endif
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

L3INT Q931ProcUnknownMessage(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom)
{
	/* TODO:  Unhandled paramaters */
	(void)pTrunk;
	(void)b;
	(void)iFrom;

    return 0;
}

L3INT Q931ProcUnexpectedMessage(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom)
{
	/* TODO:  Unhandled paramaters */
	(void)pTrunk;
	(void)b;
	(void)iFrom;

    return 0;
}

L3INT Q931Disconnect(Q931_TrunkInfo_t *pTrunk, L3INT iTo, L3INT iCRV, L3INT iCause)
{
	/* TODO:  Unhandled paramaters */
	(void)pTrunk;
	(void)iTo;
	(void)iCRV;
	(void)iCause;

    return 0;
}

L3INT Q931ReleaseComplete(Q931_TrunkInfo_t *pTrunk, L3UCHAR *buf)
{
    Q931mes_Header *ptr = (Q931mes_Header*)&buf[Q931L4HeaderSpace];
	ptr->MesType = Q931mes_RELEASE_COMPLETE;
	ptr->CRVFlag = !(ptr->CRVFlag);
	return Q931Tx32Data(pTrunk,0,buf,ptr->Size);
}

L3INT Q931AckRestart(Q931_TrunkInfo_t *pTrunk, L3UCHAR *buf)
{
	L3INT RetCode;

    Q931mes_Header *ptr = (Q931mes_Header*)&buf[Q931L4HeaderSpace];
	ptr->MesType = Q931mes_RESTART_ACKNOWLEDGE;
	//if (ptr->CRV) {
		ptr->CRVFlag = !(ptr->CRVFlag);
		//}

	RetCode = Q931Proc[pTrunk->Dialect][ptr->MesType](pTrunk, buf, 4);

    return RetCode;
}

L3INT Q931AckSetup(Q931_TrunkInfo_t *pTrunk, L3UCHAR *buf)
{
	L3INT RetCode;

    Q931mes_Header *ptr = (Q931mes_Header*)&buf[Q931L4HeaderSpace];
	ptr->MesType = Q931mes_SETUP_ACKNOWLEDGE;

	RetCode = Q931Proc[pTrunk->Dialect][ptr->MesType](pTrunk, buf, 4);

    return RetCode;
}

L3INT Q931AckConnect(Q931_TrunkInfo_t *pTrunk, L3UCHAR *buf)
{
	L3INT RetCode;

    Q931mes_Header *ptr = (Q931mes_Header*)&buf[Q931L4HeaderSpace];
	ptr->MesType = Q931mes_CONNECT_ACKNOWLEDGE;

	RetCode = Q931Proc[pTrunk->Dialect][ptr->MesType](pTrunk, buf, 4);

    return RetCode;
}

L3INT Q931AckService(Q931_TrunkInfo_t *pTrunk, L3UCHAR *buf)
{
	L3INT RetCode;

    Q931mes_Header *ptr = (Q931mes_Header*)&buf[Q931L4HeaderSpace];
	ptr->MesType = Q931mes_SERVICE_ACKNOWLEDGE;
	if (ptr->CRV) {
		ptr->CRVFlag = !(ptr->CRVFlag);
	}

	RetCode = Q931Proc[pTrunk->Dialect][ptr->MesType](pTrunk, buf, 4);

    return RetCode;
}

Q931_ENUM_NAMES(DIALECT_TYPE_NAMES, DIALECT_STRINGS)
Q931_STR2ENUM(q931_str2Q931Dialect_type, q931_Q931Dialect_type2str, Q931Dialect_t, DIALECT_TYPE_NAMES, Q931_Dialect_Count)
