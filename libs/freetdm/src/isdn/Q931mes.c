/*****************************************************************************

  FileName:	Q931mes.c

  Contents:	Pack/Unpack functions. These functions will unpack a Q931 
		message from the bit packed original format into structs
		that contains variables sized by the user. It will also pack
		the struct back into a Q.931 message as required.

		See q931.h for description. 

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

/**
 * Q931MesgHeader
 * \brief	Create Q.931 Message header
 */
L3INT Q931MesgHeader(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *mes, L3UCHAR *OBuf, L3INT Size, L3INT *IOff)
{
	L3INT Octet = *IOff;

	Q931Log(pTrunk, Q931_LOG_DEBUG, "Creating Q.931 Message Header:\n    ProtDisc %d (%#x), CRV %d (%#x), CRVflag: %d (%#x), MesType: %d (%#x)\n",
			 mes->ProtDisc, mes->ProtDisc, mes->CRV, mes->CRV, mes->CRVFlag, mes->CRVFlag, mes->MesType, mes->MesType);

	OBuf[Octet++] = mes->ProtDisc;				/* Protocol discriminator */
	if (!Q931_IS_BRI(pTrunk)) {
		OBuf[Octet++] = 2;									/* length is 2 octets */
		OBuf[Octet++] = (L3UCHAR)((mes->CRV >> 8) & 0x7f) | ((mes->CRVFlag << 7) & 0x80);	/* msb */
		OBuf[Octet++] = (L3UCHAR) (mes->CRV & 0xff);						/* lsb */
	} else {
		OBuf[Octet++] = 1;									/* length is 1 octet */
		OBuf[Octet++] = (L3UCHAR) (mes->CRV & 0x7f) | ((mes->CRVFlag << 7) & 0x80);		/* CRV & flag */
	}
	OBuf[Octet++] = mes->MesType;				/* message header */

	*IOff = Octet;
	return 0;
}


/*****************************************************************************

  Function:	 Q931Umes_Alerting

*****************************************************************************/

L3INT Q931Umes_Alerting(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_BEARER_CAPABILITY: 
		case Q931ie_CHANNEL_IDENTIFICATION:
		case Q931ie_PROGRESS_INDICATOR:
		case Q931ie_DISPLAY:
		case Q931ie_SIGNAL:
		case Q931ie_HIGH_LAYER_COMPATIBILITY:
		case Q931ie_USER_USER:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Alerting

*****************************************************************************/
L3INT Q931Pmes_Alerting(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Bearer capability */
	if (Q931IsIEPresent(pMes->BearerCap)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_BEARER_CAPABILITY](pTrunk, Q931GetIEPtr(pMes->BearerCap,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Channel Identification */
	if (Q931IsIEPresent(pMes->ChanID)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CHANNEL_IDENTIFICATION](pTrunk, Q931GetIEPtr(pMes->ChanID,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Progress indicator */
	if (Q931IsIEPresent(pMes->ProgInd)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_PROGRESS_INDICATOR](pTrunk, Q931GetIEPtr(pMes->ProgInd,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Signal */
	if (Q931IsIEPresent(pMes->Signal)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_SIGNAL](pTrunk, Q931GetIEPtr(pMes->Signal,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* High Layer Compatibility */
	if (Q931IsIEPresent(pMes->HLComp)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_HIGH_LAYER_COMPATIBILITY](pTrunk, Q931GetIEPtr(pMes->HLComp,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_CallProceeding

*****************************************************************************/
L3INT Q931Umes_CallProceeding(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_BEARER_CAPABILITY:
		case Q931ie_CHANNEL_IDENTIFICATION:
		case Q931ie_PROGRESS_INDICATOR:
		case Q931ie_DISPLAY:
		case Q931ie_HIGH_LAYER_COMPATIBILITY:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_CallProceeding

*****************************************************************************/
L3INT Q931Pmes_CallProceeding(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Bearer capability */
	if (Q931IsIEPresent(pMes->BearerCap)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_BEARER_CAPABILITY](pTrunk, Q931GetIEPtr(pMes->BearerCap,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Channel Identification */
	if (Q931IsIEPresent(pMes->ChanID)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CHANNEL_IDENTIFICATION](pTrunk, Q931GetIEPtr(pMes->ChanID,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}
	
	/* Progress indicator */
	if (Q931IsIEPresent(pMes->ProgInd)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_PROGRESS_INDICATOR](pTrunk, Q931GetIEPtr(pMes->ProgInd,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* High Layer Compatibility */
	if (Q931IsIEPresent(pMes->HLComp)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_HIGH_LAYER_COMPATIBILITY](pTrunk, Q931GetIEPtr(pMes->HLComp,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_CongestionControl

*****************************************************************************/
L3INT Q931Umes_CongestionControl(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3BOOL RetCode = L3FALSE;

	NoWarning(mes);
	NoWarning(IBuf);

	return RetCode;
}


/*****************************************************************************

  Function:	 Q931Pmes_CongestionControl

*****************************************************************************/
L3INT Q931Pmes_CongestionControl(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	L3BOOL RetCode = L3FALSE;

	NoWarning(OBuf);
	NoWarning(IBuf);

	*OSize = 0;	
	return RetCode;
}

/*****************************************************************************

  Function:	 Q931Umes_Connect

*****************************************************************************/
L3INT Q931Umes_Connect(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_BEARER_CAPABILITY:
		case Q931ie_CHANNEL_IDENTIFICATION:
		case Q931ie_PROGRESS_INDICATOR:
		case Q931ie_DISPLAY:
		case Q931ie_DATETIME:
		case Q931ie_SIGNAL:
		case Q931ie_LOW_LAYER_COMPATIBILITY:
		case Q931ie_HIGH_LAYER_COMPATIBILITY:
		case Q931ie_CONNECTED_NUMBER:		/* not actually used, seen while testing BRI PTMP TE */
		case Q931ie_USER_USER:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;

		default:
			Q931Log(pTrunk, Q931_LOG_ERROR, "Illegal IE %#hhx in Connect Message\n", IBuf[IOff]);

			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Connect

*****************************************************************************/
L3INT Q931Pmes_Connect(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Bearer capability */
	if (Q931IsIEPresent(pMes->BearerCap)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_BEARER_CAPABILITY](pTrunk, Q931GetIEPtr(pMes->BearerCap,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Channel Identification */
	if (Q931IsIEPresent(pMes->ChanID)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CHANNEL_IDENTIFICATION](pTrunk, Q931GetIEPtr(pMes->ChanID,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Progress indicator */
	if (Q931IsIEPresent(pMes->ProgInd)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_PROGRESS_INDICATOR](pTrunk, Q931GetIEPtr(pMes->ProgInd,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Date/Time */
	if (Q931IsIEPresent(pMes->DateTime)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DATETIME](pTrunk, Q931GetIEPtr(pMes->DateTime,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Signal */
	if (Q931IsIEPresent(pMes->Signal)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_SIGNAL](pTrunk, Q931GetIEPtr(pMes->Signal,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Low Layer Compatibility */
	if (Q931IsIEPresent(pMes->LLComp)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_LOW_LAYER_COMPATIBILITY](pTrunk, Q931GetIEPtr(pMes->LLComp,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* High Layer Compatibility */
	if (Q931IsIEPresent(pMes->HLComp)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_HIGH_LAYER_COMPATIBILITY](pTrunk, Q931GetIEPtr(pMes->HLComp,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_ConnectAck

*****************************************************************************/
L3INT Q931Umes_ConnectAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_DISPLAY:
		case Q931ie_SIGNAL:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}


/*****************************************************************************

  Function:	 Q931Pmes_ConnectAck

*****************************************************************************/
L3INT Q931Pmes_ConnectAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Signal */
	if (Q931IsIEPresent(pMes->Signal)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_SIGNAL](pTrunk, Q931GetIEPtr(pMes->Signal,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_Disconnect

*****************************************************************************/
L3INT Q931Umes_Disconnect(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CAUSE:
		case Q931ie_PROGRESS_INDICATOR:
		case Q931ie_DISPLAY:
		case Q931ie_SIGNAL:
		case Q931ie_FACILITY:
		case Q931ie_USER_USER:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Disconnect

*****************************************************************************/
L3INT Q931Pmes_Disconnect(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Cause */
	if (Q931IsIEPresent(pMes->Cause)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CAUSE](pTrunk, Q931GetIEPtr(pMes->Cause,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}
	
	/* Progress indicator */
	if (Q931IsIEPresent(pMes->ProgInd)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_PROGRESS_INDICATOR](pTrunk, Q931GetIEPtr(pMes->ProgInd,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Signal */
	if (Q931IsIEPresent(pMes->Signal)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_SIGNAL](pTrunk, Q931GetIEPtr(pMes->Signal,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_Information

*****************************************************************************/
L3INT Q931Umes_Information(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_SENDING_COMPLETE:
		case Q931ie_DISPLAY:
		case Q931ie_KEYPAD_FACILITY:
		case Q931ie_CALLED_PARTY_NUMBER:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Information

*****************************************************************************/
L3INT Q931Pmes_Information(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);

	/* Sending Complete */
	if (Q931IsIEPresent(pMes->SendComplete)) {
		OBuf[Octet++]	= (L3UCHAR)(pMes->SendComplete & 0x00ff);
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Keypad Facility */
	if (Q931IsIEPresent(pMes->KeypadFac)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_KEYPAD_FACILITY](pTrunk, Q931GetIEPtr(pMes->KeypadFac,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Signal */
	if (Q931IsIEPresent(pMes->Signal)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_SIGNAL](pTrunk, Q931GetIEPtr(pMes->Signal,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Called Party number */
	if (Q931IsIEPresent(pMes->CalledNum)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CALLED_PARTY_NUMBER](pTrunk, Q931GetIEPtr(pMes->CalledNum,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_Notify

*****************************************************************************/
L3INT Q931Umes_Notify(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_BEARER_CAPABILITY:
		case Q931ie_NOTIFICATION_INDICATOR:
		case Q931ie_DISPLAY:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Notify

*****************************************************************************/
L3INT Q931Pmes_Notify(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Bearer capability */
	if (Q931IsIEPresent(pMes->BearerCap)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_BEARER_CAPABILITY](pTrunk, Q931GetIEPtr(pMes->BearerCap,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_Progress

*****************************************************************************/
L3INT Q931Umes_Progress(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_BEARER_CAPABILITY:
		case Q931ie_CAUSE:
		case Q931ie_PROGRESS_INDICATOR:
		case Q931ie_DISPLAY:
		case Q931ie_HIGH_LAYER_COMPATIBILITY:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Progress

*****************************************************************************/
L3INT Q931Pmes_Progress(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Bearer capability */
	if (Q931IsIEPresent(pMes->BearerCap)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_BEARER_CAPABILITY](pTrunk, Q931GetIEPtr(pMes->BearerCap,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Cause */
	if (Q931IsIEPresent(pMes->Cause)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CAUSE](pTrunk, Q931GetIEPtr(pMes->Cause,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Progress indicator */
	if (Q931IsIEPresent(pMes->ProgInd)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_PROGRESS_INDICATOR](pTrunk, Q931GetIEPtr(pMes->ProgInd,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* High Layer Compatibility */
	if (Q931IsIEPresent(pMes->HLComp)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_HIGH_LAYER_COMPATIBILITY](pTrunk, Q931GetIEPtr(pMes->HLComp,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_Release

*****************************************************************************/
L3INT Q931Umes_Release(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CAUSE:
		case Q931ie_DISPLAY:
		case Q931ie_SIGNAL:
		case Q931ie_USER_USER:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Release

*****************************************************************************/
L3INT Q931Pmes_Release(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Cause */
	if (Q931IsIEPresent(pMes->Cause)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CAUSE](pTrunk, Q931GetIEPtr(pMes->Cause,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Signal */
	if (Q931IsIEPresent(pMes->Signal)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_SIGNAL](pTrunk, Q931GetIEPtr(pMes->Signal,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_ReleaseComplete

*****************************************************************************/
L3INT Q931Umes_ReleaseComplete(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CAUSE:
		case Q931ie_DISPLAY:
		case Q931ie_SIGNAL:
		case Q931ie_USER_USER:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_ReleaseComplete

*****************************************************************************/
L3INT Q931Pmes_ReleaseComplete(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Cause */
	if (Q931IsIEPresent(pMes->Cause)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CAUSE](pTrunk, Q931GetIEPtr(pMes->Cause,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Signal */
	if (Q931IsIEPresent(pMes->Signal)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_SIGNAL](pTrunk, Q931GetIEPtr(pMes->Signal,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_Restart

*****************************************************************************/
L3INT Q931Umes_Restart(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CHANNEL_IDENTIFICATION:
		case Q931ie_DISPLAY:
		case Q931ie_RESTART_INDICATOR:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Restart

*****************************************************************************/
L3INT Q931Pmes_Restart(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
   
	/* ChanID */
	if (Q931IsIEPresent(pMes->ChanID)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CHANNEL_IDENTIFICATION](pTrunk, Q931GetIEPtr(pMes->ChanID,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* RestartInd */
	if (Q931IsIEPresent(pMes->RestartInd)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_RESTART_INDICATOR](pTrunk, Q931GetIEPtr(pMes->RestartInd,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_RestartAck

*****************************************************************************/
L3INT Q931Umes_RestartAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT IOff, L3INT Size)
{
	Q931mes_Generic *mes = (Q931mes_Generic*)OBuf;
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CHANNEL_IDENTIFICATION:
		case Q931ie_DISPLAY:
		case Q931ie_RESTART_INDICATOR:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_RestartAck

*****************************************************************************/
L3INT Q931Pmes_RestartAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* ChanID */
	if (Q931IsIEPresent(pMes->ChanID)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CHANNEL_IDENTIFICATION](pTrunk, Q931GetIEPtr(pMes->ChanID,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* RestartInd */
	if (Q931IsIEPresent(pMes->RestartInd)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_RESTART_INDICATOR](pTrunk, Q931GetIEPtr(pMes->RestartInd,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_Resume

*****************************************************************************/
L3INT Q931Umes_Resume(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CALL_IDENTITY:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Resume

*****************************************************************************/
L3INT Q931Pmes_Resume(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);

	/* Call Identity */
	if (Q931IsIEPresent(pMes->CallID)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CALL_IDENTITY](pTrunk, Q931GetIEPtr(pMes->CallID,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_ResumeAck

*****************************************************************************/
L3INT Q931Umes_ResumeAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CHANNEL_IDENTIFICATION:
		case Q931ie_DISPLAY:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}


/*****************************************************************************

  Function:	 Q931Pmes_ResumeAck

*****************************************************************************/
L3INT Q931Pmes_ResumeAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Channel Identification */
	if (Q931IsIEPresent(pMes->ChanID)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CHANNEL_IDENTIFICATION](pTrunk, Q931GetIEPtr(pMes->ChanID,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_ResumeReject

*****************************************************************************/
L3INT Q931Umes_ResumeReject(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CAUSE:
		case Q931ie_DISPLAY:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}


/*****************************************************************************

  Function:	 Q931Pmes_ResumeReject

*****************************************************************************/
L3INT Q931Pmes_ResumeReject(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header	*/
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);

	/* Cause */
	if (Q931IsIEPresent(pMes->Cause)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CAUSE](pTrunk, Q931GetIEPtr(pMes->Cause,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

L3INT Q931Umes_Segment(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT OOff)
{
	L3INT i = IOff;

	return IOff - i;
}

L3INT Q931Pmes_Segment(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	L3BOOL RetCode = L3FALSE;

	*OSize = 0;	
	return RetCode;
}

/*****************************************************************************

  Function:	 Q931Umes_Setup

*****************************************************************************/
L3INT Q931Umes_Setup(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT ir = 0;
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_SENDING_COMPLETE:
			IOff++;
			break;

		case Q931ie_BEARER_CAPABILITY:
		case Q931ie_CHANNEL_IDENTIFICATION:
		case Q931ie_PROGRESS_INDICATOR:
		case Q931ie_NETWORK_SPECIFIC_FACILITIES:
		case Q931ie_DISPLAY:
		case Q931ie_DATETIME:
		case Q931ie_KEYPAD_FACILITY:
		case Q931ie_SIGNAL:
		case Q931ie_CALLING_PARTY_NUMBER:
		case Q931ie_CALLING_PARTY_SUBADDRESS:
		case Q931ie_CALLED_PARTY_NUMBER:
		case Q931ie_CALLED_PARTY_SUBADDRESS:
		case Q931ie_TRANSIT_NETWORK_SELECTION:
		case Q931ie_LOW_LAYER_COMPATIBILITY:
		case Q931ie_HIGH_LAYER_COMPATIBILITY:
		case Q931ie_FACILITY:
		case Q931ie_USER_USER:
		case Q931ie_REDIRECTING_NUMBER:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;

		case Q931ie_REPEAT_INDICATOR:
			if (ir < 2) {
				rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
				ir++;
			} else {
				return Q931E_ILLEGAL_IE;
			}
			break;

		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Setup

  Decription:	Pack a Q931mes_Generic into a real Q.931 message. The user will
				set up a SETUP message and issue this to the stack where it
				is processed by Q931ProcSetup that processes and validates
				it before it actually sends it out. This function is called
				to compute the real Q.931 message.

  Parameters:	IBuf[IN]	Ptr to un-packed struct
				ISize[IN]	Size of input buffer (unpacked message).
				OBuf[OUT]	Ptr to packed 'octet' wise message.
				OSize[OUT]	Size of packed message.

  Called By:	Q931ProcSetup

*****************************************************************************/
L3INT Q931Pmes_Setup(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);

	/* Sending Complete */
	if (Q931IsIEPresent(pMes->SendComplete)) {
		OBuf[Octet++] = (L3UCHAR)Q931ie_SENDING_COMPLETE & 0xff;
	}

	/* Repeat Indicator */
	if (Q931IsIEPresent(pMes->RepeatInd)) {
		OBuf[Octet++] = (L3UCHAR)Q931ie_REPEAT_INDICATOR & 0xff;		
	}

	/* Bearer capability */
	if (Q931IsIEPresent(pMes->BearerCap)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_BEARER_CAPABILITY](pTrunk, Q931GetIEPtr(pMes->BearerCap,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	} else {
		rc = Q931E_BEARERCAP;
	}

	/* Channel Identification */
	if (Q931IsIEPresent(pMes->ChanID)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CHANNEL_IDENTIFICATION](pTrunk, Q931GetIEPtr(pMes->ChanID,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Progress indicator */
	if (Q931IsIEPresent(pMes->ProgInd)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_PROGRESS_INDICATOR](pTrunk, Q931GetIEPtr(pMes->ProgInd,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Network spesific facilities */
	if (Q931IsIEPresent(pMes->NetFac)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_NETWORK_SPECIFIC_FACILITIES](pTrunk, Q931GetIEPtr(pMes->NetFac,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Date/Time */
	if (Q931IsIEPresent(pMes->DateTime)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DATETIME](pTrunk, Q931GetIEPtr(pMes->DateTime,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Keypad Facility */
	if (Q931IsIEPresent(pMes->KeypadFac)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_KEYPAD_FACILITY](pTrunk, Q931GetIEPtr(pMes->KeypadFac,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Signal */
	if (Q931IsIEPresent(pMes->Signal)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_SIGNAL](pTrunk, Q931GetIEPtr(pMes->Signal,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Calling Party Number */
	if (Q931IsIEPresent(pMes->CallingNum)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CALLING_PARTY_NUMBER](pTrunk, Q931GetIEPtr(pMes->CallingNum,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Calling Party Subaddress */
	if (Q931IsIEPresent(pMes->CallingSub)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CALLING_PARTY_SUBADDRESS](pTrunk, Q931GetIEPtr(pMes->CallingSub,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Called Party number */
	if (Q931IsIEPresent(pMes->CalledNum)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CALLED_PARTY_NUMBER](pTrunk, Q931GetIEPtr(pMes->CalledNum,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Called party subaddress */
	if (Q931IsIEPresent(pMes->CalledSub)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CALLED_PARTY_SUBADDRESS](pTrunk, Q931GetIEPtr(pMes->CalledSub,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Transit network selection */
	if (Q931IsIEPresent(pMes->TransNetSel)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_TRANSIT_NETWORK_SELECTION](pTrunk, Q931GetIEPtr(pMes->TransNetSel,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Repeat Indicator */
	if (Q931IsIEPresent(pMes->LLRepeatInd)) {
		rc = Q931E_UNKNOWN_IE;/* TODO */
	}

	/* Low Layer Compatibility */
	if (Q931IsIEPresent(pMes->LLComp)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_LOW_LAYER_COMPATIBILITY](pTrunk, Q931GetIEPtr(pMes->LLComp,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* High Layer Compatibility */
	if (Q931IsIEPresent(pMes->HLComp)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_HIGH_LAYER_COMPATIBILITY](pTrunk, Q931GetIEPtr(pMes->HLComp,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_SetupAck

*****************************************************************************/
L3INT Q931Umes_SetupAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CHANNEL_IDENTIFICATION:
		case Q931ie_PROGRESS_INDICATOR:
		case Q931ie_DISPLAY:
		case Q931ie_SIGNAL:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_SetupAck

*****************************************************************************/
L3INT Q931Pmes_SetupAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Channel Identification */
	if (Q931IsIEPresent(pMes->ChanID)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CHANNEL_IDENTIFICATION](pTrunk, Q931GetIEPtr(pMes->ChanID,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Progress indicator */
	if (Q931IsIEPresent(pMes->ProgInd)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_PROGRESS_INDICATOR](pTrunk, Q931GetIEPtr(pMes->ProgInd,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Signal */
	if (Q931IsIEPresent(pMes->Signal)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_SIGNAL](pTrunk, Q931GetIEPtr(pMes->Signal,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_Status

*****************************************************************************/
L3INT Q931Umes_Status(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CAUSE:
		case Q931ie_CALL_STATE:
		case Q931ie_DISPLAY:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}


/*****************************************************************************

  Function:	 Q931Pmes_Status

*****************************************************************************/
L3INT Q931Pmes_Status(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Cause */
	if (Q931IsIEPresent(pMes->Cause)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CAUSE](pTrunk, Q931GetIEPtr(pMes->Cause,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Call State */
	if (Q931IsIEPresent(pMes->CallState)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CALL_STATE](pTrunk, Q931GetIEPtr(pMes->CallState,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_StatusEnquiry

*****************************************************************************/
L3INT Q931Umes_StatusEnquiry(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_DISPLAY:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_StatusEnquiry

*****************************************************************************/
L3INT Q931Pmes_StatusEnquiry(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	

	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_Suspend

*****************************************************************************/
L3INT Q931Umes_Suspend(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CALL_IDENTITY:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Suspend

*****************************************************************************/
L3INT Q931Pmes_Suspend(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Call Identity */
	if (Q931IsIEPresent(pMes->CallID)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CALL_IDENTITY](pTrunk, Q931GetIEPtr(pMes->CallID,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_SuspendAck

*****************************************************************************/
L3INT Q931Umes_SuspendAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_DISPLAY:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_SuspendAck

*****************************************************************************/
L3INT Q931Pmes_SuspendAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_SuspendReject

*****************************************************************************/
L3INT Q931Umes_SuspendReject(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CAUSE:
		case Q931ie_DISPLAY:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_SuspendReject

*****************************************************************************/
L3INT Q931Pmes_SuspendReject(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Cause */
	if (Q931IsIEPresent(pMes->Cause)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CAUSE](pTrunk, Q931GetIEPtr(pMes->Cause,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_UserInformation

*****************************************************************************/
L3INT Q931Umes_UserInformation(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT I, L3INT O)
{
	L3BOOL RetCode = L3FALSE;

	NoWarning(mes);
	NoWarning(IBuf);

	return RetCode;
}

/*****************************************************************************

  Function:	 Q931Pmes_UserInformation

*****************************************************************************/
L3INT Q931Pmes_UserInformation(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	L3BOOL RetCode = L3FALSE;

	NoWarning(OBuf);
	NoWarning(IBuf);

	*OSize = 0;	

	return RetCode;
}

/*****************************************************************************

  Function:	 Q931Umes_Service

*****************************************************************************/
L3INT Q931Umes_Service(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CHANNEL_IDENTIFICATION:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		case Q931ie_CHANGE_STATUS:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Service

*****************************************************************************/
L3INT Q931Pmes_Service(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Display */
	if (Q931IsIEPresent(pMes->ChanID)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CHANNEL_IDENTIFICATION](pTrunk, Q931GetIEPtr(pMes->ChanID,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	if (Q931IsIEPresent(pMes->ChangeStatus)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CHANGE_STATUS](pTrunk, Q931GetIEPtr(pMes->ChangeStatus,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}

/*****************************************************************************

  Function:	 Q931Umes_ServiceAck

*****************************************************************************/
L3INT Q931Umes_ServiceAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;

	while (IOff < Size) {
		switch (IBuf[IOff]) {
		case Q931ie_CHANNEL_IDENTIFICATION:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		case Q931ie_CHANGE_STATUS:
			rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
			if (rc != Q931E_NO_ERROR) 
				return rc;
			break;
		default:
			return Q931E_ILLEGAL_IE;
			break;
		}
	}

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_ServiceAck

*****************************************************************************/
L3INT Q931Pmes_ServiceAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	if (Q931IsIEPresent(pMes->ChangeStatus)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CHANGE_STATUS](pTrunk, Q931GetIEPtr(pMes->ChangeStatus,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	if (Q931IsIEPresent(pMes->ChanID)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CHANNEL_IDENTIFICATION](pTrunk, Q931GetIEPtr(pMes->ChanID,pMes->buf), OBuf, &Octet)) != 0)
			return rc;
	}

	*OSize = Octet;	
	return rc;
}
