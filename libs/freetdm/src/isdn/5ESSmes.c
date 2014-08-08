/*****************************************************************************

  FileName:	5ESSmes.c

  Contents:	Pack/Unpack functions. These functions will unpack a 5ESS ISDN
		message from the bit packed original format into structs
		that contains variables sized by the user. It will also pack
		the struct back into a Q.931 message as required.

		See 5ESS.h for description. 

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

  Function:	 ATT5ESSUmes_Setup

*****************************************************************************/
L3INT ATT5ESSUmes_Setup(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT ir = 0;
	L3INT OOff = 0;
	L3INT rc = Q931E_NO_ERROR;
	L3UCHAR last_codeset = 0, codeset = 0;
	L3UCHAR shift_nolock = 1;

	while (IOff < Size) {

		if (shift_nolock) {
			codeset = last_codeset;
		}

		if ((IBuf[IOff] & 0xF0) == Q931ie_SHIFT) {
			shift_nolock = (IBuf[IOff] & 0x08);
			if (shift_nolock) {
				last_codeset = codeset;
			}
			codeset = ((IBuf[IOff] & 0x07));
			IOff++;
		}

		if (codeset == 0) {
			switch (IBuf[IOff])
			{
			case Q931ie_SENDING_COMPLETE:
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
		} else if (codeset == 6) {
			switch (IBuf[IOff])
			{
			case Q931ie_GENERIC_DIGITS:
				rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
				if (rc != Q931E_NO_ERROR) 
					return rc;
				break;
			default:
				return Q931E_ILLEGAL_IE;
				break;
			}
		} else if (codeset == 7) {
			switch (IBuf[IOff])
			{
			case Q931ie_DISPLAY:
				rc = Q931Uie[pTrunk->Dialect][IBuf[IOff]](pTrunk, mes, &IBuf[IOff], &mes->buf[OOff], &IOff, &OOff);
				if (rc != Q931E_NO_ERROR) 
					return rc;
				break;
			default:
				return Q931E_ILLEGAL_IE;
				break;
			}
		} else {
			return Q931E_ILLEGAL_IE;
		}
	}
	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 ATT5ESSPmes_Setup

  Decription:   Pack a Q931mes_Generic into a real Q.931 message. The user will
				set up a SETUP message and issue this to the stack where it
				is processed by Q931ProcSetup that processes and validates
				it before it actually sends it out. This function is called
				to compute the real Q.931 message.

  Parameters:   IBuf[IN]	Ptr to un-packed struct
				ISize[IN]   Size of input buffer (unpacked message).
				OBuf[OUT]   Ptr to packed 'octet' wise message.
				OSize[OUT]  Size of packed message.

  Called By:	Q931ProcSetup

*****************************************************************************/
L3INT ATT5ESSPmes_Setup(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	L3INT rc = Q931E_NO_ERROR;
	Q931mes_Generic *pMes = (Q931mes_Generic *)IBuf;
	L3INT Octet = 0;

	/* Q931 Message Header */
	Q931MesgHeader(pTrunk, pMes, OBuf, *OSize, &Octet);
	
	/* Sending Complete				*/
	if (Q931IsIEPresent(pMes->SendComplete)) {
		OBuf[Octet++]	= (L3UCHAR)(pMes->SendComplete & 0x00ff);
	}

	/* Repeat Indicator */
	if (Q931IsIEPresent(pMes->RepeatInd)) {
		OBuf[Octet++]	= (L3UCHAR)(pMes->RepeatInd & 0x00ff);
	}

	/* Bearer capability */
	if (Q931IsIEPresent(pMes->BearerCap)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_BEARER_CAPABILITY](pTrunk, Q931GetIEPtr(pMes->BearerCap,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	} else {
		rc = Q931E_BEARERCAP;
	}

	/* Channel Identification */
	if (Q931IsIEPresent(pMes->ChanID)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CHANNEL_IDENTIFICATION](pTrunk, Q931GetIEPtr(pMes->ChanID,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	/* Progress indicator */
	if (Q931IsIEPresent(pMes->ProgInd)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_PROGRESS_INDICATOR](pTrunk, Q931GetIEPtr(pMes->ProgInd,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	/* Network specific facilities */
	if (Q931IsIEPresent(pMes->NetFac)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_NETWORK_SPECIFIC_FACILITIES](pTrunk, Q931GetIEPtr(pMes->NetFac,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	/* Display */
	if (Q931IsIEPresent(pMes->Display)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DISPLAY](pTrunk, Q931GetIEPtr(pMes->Display,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	/* Date/Time */
	if (Q931IsIEPresent(pMes->DateTime)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_DATETIME](pTrunk, Q931GetIEPtr(pMes->DateTime,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	/* Keypad Facility */
	if (Q931IsIEPresent(pMes->KeypadFac)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_KEYPAD_FACILITY](pTrunk, Q931GetIEPtr(pMes->KeypadFac,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	/* Signal */
	if (Q931IsIEPresent(pMes->Signal)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_SIGNAL](pTrunk, Q931GetIEPtr(pMes->Signal,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	/* Calling Party Number */
	if (Q931IsIEPresent(pMes->CallingNum)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CALLING_PARTY_NUMBER](pTrunk, Q931GetIEPtr(pMes->CallingNum,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	/* Calling Party Subaddress */
	if (Q931IsIEPresent(pMes->CallingSub)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CALLING_PARTY_SUBADDRESS](pTrunk, Q931GetIEPtr(pMes->CallingSub,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	/* Called Party number */
	if (Q931IsIEPresent(pMes->CalledNum)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CALLED_PARTY_NUMBER](pTrunk, Q931GetIEPtr(pMes->CalledNum,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	/* Called party subaddress */
	if (Q931IsIEPresent(pMes->CalledSub)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_CALLED_PARTY_SUBADDRESS](pTrunk, Q931GetIEPtr(pMes->CalledSub,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	/* Transit network selection */
	if (Q931IsIEPresent(pMes->TransNetSel)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_TRANSIT_NETWORK_SELECTION](pTrunk, Q931GetIEPtr(pMes->TransNetSel,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	/* Repeat Indicator */
	if (Q931IsIEPresent(pMes->LLRepeatInd)) {
		rc = Q931E_UNKNOWN_IE;/* TODO */
	}

	/* Low Layer Compatibility */
	if (Q931IsIEPresent(pMes->LLComp)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_LOW_LAYER_COMPATIBILITY](pTrunk, Q931GetIEPtr(pMes->LLComp,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	/* High Layer Compatibility */
	if (Q931IsIEPresent(pMes->HLComp)) {
		if ((rc = Q931Pie[pTrunk->Dialect][Q931ie_HIGH_LAYER_COMPATIBILITY](pTrunk, Q931GetIEPtr(pMes->HLComp,pMes->buf), OBuf, &Octet))!=0)
			return rc;
	}

	*OSize = Octet;
	return rc;
}


/*****************************************************************************

  Function:	 ATT5ESSUmes_0x0f

*****************************************************************************/
L3INT ATT5ESSUmes_0x0f(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	if (mes->ProtDisc == 8) {
		return Q931Umes_ConnectAck(pTrunk, IBuf, mes, IOff, Size);
	}

	if (mes->ProtDisc == 3) {
		return Q931Umes_Service(pTrunk, IBuf, mes, IOff, Size);
	}

	return Q931E_UNKNOWN_MESSAGE;
}

/*****************************************************************************

  Function:	 ATT5ESSPmes_0x0f

*****************************************************************************/
L3INT ATT5ESSPmes_0x0f(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *mes = (Q931mes_Generic *)IBuf;

	if (mes->ProtDisc == 8) {
		return Q931Pmes_ConnectAck(pTrunk, IBuf, ISize, OBuf, OSize);
	}

	if (mes->ProtDisc == 3) {
		return Q931Pmes_Service(pTrunk, IBuf, ISize, OBuf, OSize);
	}

	return Q931E_UNKNOWN_MESSAGE;
}

/*****************************************************************************

  Function:	 ATT5ESSUmes_0x07

*****************************************************************************/
L3INT ATT5ESSUmes_0x07(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	if (mes->ProtDisc == 8) {
		return Q931Umes_Connect(pTrunk, IBuf, mes, IOff, Size);
	}

	if (mes->ProtDisc == 3) {
		return Q931Umes_ServiceAck(pTrunk, IBuf, mes, IOff, Size);
	}

	return Q931E_UNKNOWN_MESSAGE;
}

/*****************************************************************************

  Function:	 ATT5ESSPmes_0x07

*****************************************************************************/
L3INT ATT5ESSPmes_0x07(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	Q931mes_Generic *mes = (Q931mes_Generic *)IBuf;

	if (mes->ProtDisc == 8) {
		return Q931Pmes_Connect(pTrunk, IBuf, ISize, OBuf, OSize);
	}

	if (mes->ProtDisc == 3) {
		return Q931Pmes_ServiceAck(pTrunk, IBuf, ISize, OBuf, OSize);
	}

	return Q931E_UNKNOWN_MESSAGE;
}
