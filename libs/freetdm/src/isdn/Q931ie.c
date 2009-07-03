/*****************************************************************************

  FileName:	 Q931ie.c

  Contents:	 Information Element Pack/Unpack functions. 
  
		These functions will pack out a Q931 message from the bit 
		packed original format into structs that are easier to process 
		and pack the same structs back into bit fields when sending
		messages out.

		The messages contains a short for each possible IE. The MSB 
		bit flags the precense of an IE, while the remaining bits 
		are the offset into a buffer to find the actual IE.

		Each IE are supported by 3 functions:

		Q931Pie_XXX	 Pack struct into Q.931 IE
		Q931Uie_XXX	 Unpack Q.931 IE into struct
		Q931InitIEXXX   Initialize IE (see Q931api.c).

  Dialect Note: This file will only contain standard DSS1 IE. Other IE as 
		used in QSIG, NI2, Q.932 etc are located in separate files.

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

#ifdef _MSC_VER
#ifndef __inline__
#define __inline__ __inline
#endif
#if (_MSC_VER >= 1400)			/* VC8+ */
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#endif
#ifndef strcasecmp
#define strcasecmp(s1, s2) _stricmp(s1, s2)
#endif
#ifndef strncasecmp
#define strncasecmp(s1, s2, n) _strnicmp(s1, s2, n)
#endif
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

/*****************************************************************************

  Macro:		Q931MoreIE

  Description:  Local helper macro detecting if there is more IE space left
		based on the 3 standard parameters Octet, Off and IESpace.
		This can be used to test if the IE is completed to avoid
		that the header of the next IE is interpreted as a part of
		the current IE.

*****************************************************************************/
#define Q931MoreIE() (Octet + Off - 2 < IESize)

#define Q931IESizeTest(x) {\
	if (Octet + Off - 2 != IESize) {\
		Q931SetError(pTrunk, x, Octet, Off);\
		return x;\
	}\
}

/*****************************************************************************

  Function:	 Q931ReadExt

  Description:  Many of the octets in the standard have an MSB 'ext.1'. This
				means that the octet usually is the latest octet, but that a
				futhure standard may extend the octet. A stack must be able 
				to handle such extensions by skipping the extension octets.

				This function will increase the offset counter with 1 for 
				each octet with an MSB of zero. This will allow the stack to
				skip extensions wihout knowing anything about them.

  Parameters:   IBuf	ptr to octet array.
				Off	 Starting offset counter

  Return Value: New offset value.

*****************************************************************************/

L3INT Q931ReadExt(L3UCHAR * IBuf, L3INT Off)
{
	L3INT c = 0;
	while ((IBuf[c] & 0x80) == 0) {
		c++;
	}
	return Off + c;
}

/*****************************************************************************

  Function:	 Q931Uie_BearerCap

  Description:  Unpack a bearer capability ie.

  Parameters:   pIE[OUT]	ptr to Information Element id.
		IBuf[IN]	ptr to a packed ie.
		OBuf[OUT]	ptr to buffer for Unpacked ie.
		IOff[IN\OUT]	Input buffer offset
		OOff[IN\OUT]	Output buffer offset

		Ibuf and OBuf points directly to buffers. The IOff and OOff
		must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/

L3INT Q931Uie_BearerCap(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff) 
{
	Q931ie_BearerCap *pie = (Q931ie_BearerCap*)OBuf;
	ie *pIE = &pMsg->BearerCap;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;
	*pIE = 0;

	/* Octet 1 */
	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	pie->CodStand = ieGetOctet((IBuf[Octet] & 0x60) >> 5);
	pie->ITC      = ieGetOctet(IBuf[Octet] & 0x1f);
	Off           = Q931ReadExt(&IBuf[Octet], Off);
	Octet++;

	/* Octet 4 */
	pie->TransMode = ieGetOctet((IBuf[Octet + Off] & 0x60) >> 5);
	pie->ITR       = ieGetOctet(IBuf[Octet + Off] & 0x1f);
	Off            = Q931ReadExt(&IBuf[Octet + Off], Off);
	Octet++;

	/* Octet 4.1. Rate multiplier is only present if ITR = Multirate		*/
	if (pie->ITR == 0x18) {
		pie->RateMul = ieGetOctet(IBuf[Octet + Off] & 0x7f);
		Off = Q931ReadExt(&IBuf[Octet + Off], Off);
		Off ++;
	}

	/* Octet 5 */
	if ((IBuf[Octet + Off] & 0x60) == 0x20 && Q931MoreIE()) {
		pie->Layer1Ident = ieGetOctet((IBuf[Octet + Off] & 0x60) >> 5);
		pie->UIL1Prot    = ieGetOctet(IBuf[Octet + Off] & 0x1f);
		Octet++;

		/* Octet 5a. The octet may be present if ITC is unrestrictd digital info
		 * and UIL1Prot is either V.110, I.460 and X.30 or V.120. It may also
		 * be present if ITC = 3.1 kHz audio and UIL1Prot is G.711.
		 * Bit 8 of Octet 5 = 0 indicates that 5a is present.
		 */

		if (IsQ931Ext(IBuf[Octet + Off - 1])) {
			if (((pie->ITC == 0x08) && (pie->UIL1Prot == 0x01 || pie->UIL1Prot == 0x08))
			|| ((pie->ITC == 0x10) && (pie->UIL1Prot == 0x02 || pie->UIL1Prot == 0x03))) {
				pie->SyncAsync = ieGetOctet((IBuf[Octet + Off] & 0x40) >> 6);
				pie->Negot     = ieGetOctet((IBuf[Octet + Off] & 0x20) >> 5);
				pie->UserRate  = ieGetOctet(IBuf[Octet + Off] & 0x1f);
				Off ++;
			}
			else {
				/* We have detected bit 8 = 0, but no setting that require the  */
				/* additional octets ??? */
				Q931SetError(pTrunk, Q931E_BEARERCAP, 5,Off);
				return Q931E_BEARERCAP;
			}

			/* Octet 5b. Two different structures used. */
			if (IsQ931Ext(IBuf[Octet + Off - 1])) {
				if (pie->UIL1Prot == 0x01) { /* ITU V.110, I.460 and X.30 */
					pie->InterRate = ieGetOctet((IBuf[Octet + Off] & 0x60) >> 5);
					pie->NIConTx   = ieGetOctet((IBuf[Octet + Off] & 0x10) >> 4);
					pie->NIConRx   = ieGetOctet((IBuf[Octet + Off] & 0x08) >> 3);
					pie->FlowCtlTx = ieGetOctet((IBuf[Octet + Off] & 0x04) >> 2);
					pie->FlowCtlRx = ieGetOctet((IBuf[Octet + Off] & 0x20) >> 1);
					Off++;
				}
				else if (pie->UIL1Prot == 0x08) { /* ITU V.120 */
					pie->HDR        = ieGetOctet((IBuf[Octet + Off] & 0x40) >> 6);
					pie->MultiFrame = ieGetOctet((IBuf[Octet + Off] & 0x20) >> 5);
					pie->Mode       = ieGetOctet((IBuf[Octet + Off] & 0x10) >> 4);
					pie->LLInegot   = ieGetOctet((IBuf[Octet + Off] & 0x08) >> 3);
					pie->Assignor   = ieGetOctet((IBuf[Octet + Off] & 0x04) >> 2);
					pie->InBandNeg  = ieGetOctet((IBuf[Octet + Off] & 0x02) >> 1);
					Off++;
				}
				else {
					Q931SetError(pTrunk,Q931E_BEARERCAP, 5,Off);
					return Q931E_BEARERCAP;
				}

				/* Octet 5c */
				if (IsQ931Ext(IBuf[Octet + Off - 1])) {
					pie->NumStopBits = ieGetOctet((IBuf[Octet + Off] & 0x60) >> 5);
					pie->NumDataBits = ieGetOctet((IBuf[Octet + Off] & 0x18) >> 3);
					pie->Parity      = ieGetOctet(IBuf[Octet + Off] & 0x07);
					Off++;

					/* Octet 5d */
					if (IsQ931Ext(IBuf[Octet + Off - 1])) {
						pie->DuplexMode = ieGetOctet((IBuf[Octet + Off] & 0x40) >> 6);
						pie->ModemType  = ieGetOctet(IBuf[Octet + Off] & 0x3f);
						Off ++;
					}
				}
			}
		}
	}

	/* Octet 6 */
	if ((IBuf[Octet + Off] & 0x60) == 0x40 && Q931MoreIE()) {
		pie->Layer2Ident = ieGetOctet((IBuf[Octet + Off] & 0x60) >> 5);
		pie->UIL2Prot    = ieGetOctet(IBuf[Octet + Off] & 0x1f);

		Off = Q931ReadExt(&IBuf[Octet + Off], Off);
		Octet ++;
	}

	/* Octet 7 */
	if ((IBuf[Octet + Off] & 0x60) == 0x60 && Q931MoreIE()) {
		pie->Layer3Ident = ieGetOctet((IBuf[Octet + Off] & 0x60) >> 5);
		pie->UIL3Prot    = ieGetOctet(IBuf[Octet + Off] & 0x1f);
		Octet++;

		/* Octet 7a */
		if (IsQ931Ext(IBuf[Octet + Off - 1])) {
			if (pie->UIL3Prot == 0x0c) {
				pie->AL3Info1 = ieGetOctet(IBuf[Octet + Off] & 0x0f);
				Off++;

				/* Octet 7b */
				if (IsQ931Ext(IBuf[Octet + Off])) {
					pie->AL3Info2 = ieGetOctet(IBuf[Octet + Off] & 0x0f);
					Off++;
				}
			}
			else {
				Q931SetError(pTrunk,Q931E_BEARERCAP, 7, Off);
				return Q931E_BEARERCAP;

			}
		}
	}

	Q931IESizeTest(Q931E_BEARERCAP);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_BearerCap);
	pie->Size = sizeof(Q931ie_BearerCap);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_BearerCap

  Description:  Packing a Q.931 Bearer Capability element from a generic 
				struct into a packed octet structure in accordance with the
				standard.

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/

L3INT Q931Pie_BearerCap(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_BearerCap *pIE = (Q931ie_BearerCap*)IBuf;
	L3INT rc = 0;
	L3INT Beg = *Octet; /* remember current offset */
	L3INT li;

	Q931Log(pTrunk, Q931_LOG_DEBUG, "Encoding Bearer Capability IE\n");

	OBuf[(*Octet)++] = Q931ie_BEARER_CAPABILITY ;
	li = (*Octet)++; /* remember length position */

	/* Octet 3 - Coding standard / Information transfer capability */
	OBuf[(*Octet)++] = 0x80 | ((pIE->CodStand << 5) & 0x60) | (pIE->ITC & 0x1f);

	/* Octet 4 - Transfer mode / Information transfer rate */
	OBuf[(*Octet)++] = 0x80 | ((pIE->TransMode << 5) & 0x60) | (pIE->ITR & 0x1f);

	if (pIE->ITR == 0x18) {
		/* Octet 4.1 - Rate Multiplier */
		OBuf[(*Octet)++] = 0x80 | (pIE->RateMul & 0x7f);
	}

	/* Octet 5 - Layer 1 Ident / User information layer 1 protocol */
	if (pIE->Layer1Ident == 0x01) {
		if (((pIE->ITC == 0x08) && (pIE->UIL1Prot == 0x01 || pIE->UIL1Prot == 0x08)) ||
		   ((pIE->ITC == 0x10) && (pIE->UIL1Prot == 0x02 || pIE->UIL1Prot == 0x03))) {
			OBuf[(*Octet)++] = 0x00 | ((pIE->Layer1Ident << 5) & 0x60) | (pIE->UIL1Prot & 0x15);
			
			/* Octet 5a - SyncAsync/Negot/UserRate */
			OBuf[(*Octet)++] = 0x00 | ((pIE->SyncAsync << 6) & 0x40) | ((pIE->Negot << 5) & 0x20) | (pIE->UserRate & 0x1f);

			/* Octet 5b - one of two types */
			if (pIE->UIL1Prot == 0x01) { /* ITU V.110, I.460 and X.30	*/
				/* Octet 5b - Intermed rate/ Nic on Tx/Nix on Rx/FlowCtlTx/FlowCtlRx */
				OBuf[(*Octet)++] = 0x00 
						| ((pIE->InterRate << 6) & 0x60)
						| ((pIE->NIConTx   << 4) & 0x10)
						| ((pIE->NIConRx   << 3) & 0x08)
						| ((pIE->FlowCtlTx << 2) & 0x04)
						| ((pIE->FlowCtlRx << 1) & 0x02);
			}
			else if (pIE->UIL1Prot == 0x08) { /* ITU V.120 */
				/* Octet 5b - HDR/Multiframe/Mode/LLINegot/Assignor/Inbandneg*/
				OBuf[(*Octet)++] = 0x00
						| ((pIE->InterRate  << 6) & 0x60)
						| ((pIE->MultiFrame << 5) & 0x20)
						| ((pIE->Mode       << 4) & 0x10)
						| ((pIE->LLInegot   << 3) & 0x08)
						| ((pIE->Assignor   << 2) & 0x04)
						| ((pIE->InBandNeg  << 1) & 0x02);
			}

			/* Octet 5c - NumStopBits/NumStartBits/Parity					*/
			OBuf[(*Octet)++] = 0x00
					| ((pIE->NumStopBits << 5) & 0x60)
					| ((pIE->NumDataBits << 3) & 0x18)
					| (pIE->Parity & 0x07);

			/* Octet 5d - Duplex Mode/Modem Type */
			OBuf[(*Octet)++] = 0x80 | ((pIE->DuplexMode << 6) & 0x40) | (pIE->ModemType & 0x3f);
		}
		else {
			OBuf[(*Octet)++] = 0x80 | ((pIE->Layer1Ident << 5) & 0x60) | (pIE->UIL1Prot & 0x1f);
		}
	}

	/* Octet 6 - Layer2Ident/User information layer 2 prtocol */
	if (pIE->Layer2Ident == 0x02) {
		OBuf[(*Octet)++] = 0x80 | ((pIE->Layer2Ident << 5) & 0x60) | (pIE->UIL2Prot & 0x1f);
	}

	/* Octet 7 - Layer 3 Ident/ User information layer 3 protocol */
	if (pIE->Layer3Ident == 0x03) {
		if (pIE->UIL3Prot == 0x0c) {
			OBuf[(*Octet)++] = 0x00 | ((pIE->Layer3Ident << 5) & 0x60) | (pIE->UIL3Prot & 0x1f);

			/* Octet 7a - Additional information layer 3 msb */
			OBuf[(*Octet)++] = 0x00 | (pIE->AL3Info1 & 0x0f);

			/* Octet 7b - Additional information layer 3 lsb */
			OBuf[(*Octet)++] = 0x80 | (pIE->AL3Info2 & 0x0f);
		}
		else {
			OBuf[(*Octet)++] = 0x80 | ((pIE->Layer3Ident << 5) & 0x60) | (pIE->UIL3Prot & 0x1f);
		}
	}

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_CallID

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset


				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_CallID(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_CallID *pie = (Q931ie_CallID*)OBuf;
	ie *pIE = &pMsg->CallID;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT x = 0;
	L3INT IESize;

	*pIE = 0;

	/* Octet 1 */
	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	do {
		pie->CallId[x] = IBuf[Octet + Off] & 0x7f;
		Off++;
		x++;
	} while (Q931MoreIE());

	Q931IESizeTest(Q931E_CALLID);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_CallID) + x - 1;
	pie->Size = (L3UCHAR)(sizeof(Q931ie_CallID) + x - 1);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_CallID

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/

L3INT Q931Pie_CallID(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_CallID *pIE = (Q931ie_CallID*)IBuf;
	L3INT rc = 0;
	L3INT Beg = *Octet;/* remember current offset */
	L3INT li;
	L3INT sCI = pIE->Size - sizeof(Q931ie_CallID) + 1;
	L3INT x;

	OBuf[(*Octet)++] = Q931ie_CALL_IDENTITY ;
	li = (*Octet)++; /* remember length position */

	for (x = 0; x < sCI; x++) {
		OBuf[(*Octet)++] = pIE->CallId[x];
	}

	OBuf[(*Octet) - 1] |= 0x80; /* set complete flag at last octet*/

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_CallState

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_CallState(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_CallState *pie = (Q931ie_CallState*)OBuf;
	ie *pIE = &pMsg->CallState;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;

	*pIE = 0;

	/* Octet 1 */
	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++];

	/* Octet 3 */
	pie->CodStand  = (IBuf[Octet + Off] >> 6) & 0x03;
	pie->CallState =  IBuf[Octet + Off] & 0x3f;
	Octet++;

	Q931IESizeTest(Q931E_CALLSTATE);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_CallState);
	pie->Size = sizeof(Q931ie_CallState);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_CallState

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value: Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_CallState(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_CallState *pIE = (Q931ie_CallState*)IBuf;
	L3INT rc = 0;
	L3INT Beg = *Octet; /* remember current offset */
	L3INT li;

	OBuf[(*Octet)++] = Q931ie_CALL_STATE;
	li = (*Octet)++; /* remember length position */

	OBuf[(*Octet)++] = (pIE->CodStand << 6) | (pIE->CallState & 0x3f);

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_CalledSub

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_CalledSub(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_CalledSub *pie = (Q931ie_CalledSub*)OBuf;
	ie *pIE = &pMsg->CalledSub;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT x;
	L3INT IESize;

	*pIE = 0;

	/* Octet 1 */
	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	pie->TypNum     = (IBuf[Octet + Off] >> 4) & 0x07;
	pie->OddEvenInd = (IBuf[Octet + Off] >> 3) & 0x01;
	Octet++;
	
	/* Octet 4 */
	x = 0;
	do {
		pie->Digit[x] = IBuf[Octet + Off] & 0x7f;
		Off++;
		x++;
	} while (Q931MoreIE() && x < 20);

	Q931IESizeTest(Q931E_CALLEDSUB);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_CalledSub) + x - 1;
	pie->Size = (L3UCHAR)(sizeof(Q931ie_CalledSub) + x - 1);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_CalledSub

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_CalledSub(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_CalledSub *pIE = (Q931ie_CalledSub*)IBuf;
	L3INT rc = 0;
	L3INT Beg = *Octet;
	L3INT li;
	L3INT sN = pIE->Size - sizeof(Q931ie_CalledSub) + 1;
	L3INT x;

	/* Octet 1 */
	OBuf[(*Octet)++] = Q931ie_CALLED_PARTY_SUBADDRESS;
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = 0x80 | (pIE->TypNum << 4) | (pIE->OddEvenInd << 3);
	
	/* Octet 4 */
	for (x = 0; x<sN; x++) {
		OBuf[(*Octet)++] = pIE->Digit[x];
	}

	OBuf[(*Octet) - 1] |= 0x80; /* Terminate bit */

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_CalledNum

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_CalledNum(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_CalledNum *pie = (Q931ie_CalledNum*)OBuf;
	ie *pIE = &pMsg->CalledNum;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT x;
	L3INT IESize; /* # digits in this case */

	*pIE = 0;

	/* Octet 1 */
	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	pie->TypNum    = (IBuf[Octet + Off] >> 4) & 0x07;
	pie->NumPlanID =  IBuf[Octet + Off] & 0x0f;
	Octet++;

	/* Octet 4*/
	x = 0;
	do {
		pie->Digit[x] = IBuf[Octet + Off] & 0x7f;
		Off++;
		x++;
	} while ((IBuf[Octet + Off]&0x80) == 0 && Q931MoreIE());

	pie->Digit[x] = '\0';

	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_CalledNum) + x;
	pie->Size = (L3UCHAR)(sizeof(Q931ie_CalledNum) + x);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_CalledNum

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_CalledNum(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_CalledNum *pIE = (Q931ie_CalledNum*)IBuf;
	L3INT rc = 0;
	L3INT Beg = *Octet;
	L3INT li;
	L3INT sN = pIE->Size - sizeof(Q931ie_CalledNum);
	L3INT x;

	/* Octet 1 */
	OBuf[(*Octet)++] = Q931ie_CALLED_PARTY_NUMBER;
	
	/* Octet 2 */
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = 0x80 | (pIE->TypNum << 4) | (pIE->NumPlanID);
	
	/* Octet 4 */
	for (x = 0; x<sN; x++) {
		OBuf[(*Octet)++] = pIE->Digit[x];
	}

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_CallingNum

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_CallingNum(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_CallingNum *pie = (Q931ie_CallingNum*)OBuf;
	ie *pIE = &pMsg->CallingNum;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT x;
	L3INT IESize;

	*pIE = 0;

	/* Octet 1 */
	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	pie->TypNum    = (IBuf[Octet + Off] >> 4) & 0x07;
	pie->NumPlanID =  IBuf[Octet + Off] & 0x0f;

	/* Octet 3a */
	if ((IBuf[Octet + Off] & 0x80) == 0) {
		Off++;
		pie->PresInd   = (IBuf[Octet + Off] >> 5) & 0x03;
		pie->ScreenInd =  IBuf[Octet + Off] & 0x03;
	}
	Octet++;

	/* Octet 4 */
	x = 0;
	while (Q931MoreIE()) {
		pie->Digit[x++] = IBuf[Octet + Off] & 0x7f;

		if ((IBuf[Octet + Off] & 0x80) != 0) {
			break;
		}
		Off++;
	}
	pie->Digit[x] = '\0';

	Q931IESizeTest(Q931E_CALLINGNUM);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_CallingNum) + x;
	pie->Size = (L3UCHAR)(sizeof(Q931ie_CallingNum) + x);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_CallingNum

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_CallingNum(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_CallingNum *pIE = (Q931ie_CallingNum*)IBuf;
	L3INT rc = 0;
	L3INT Beg = *Octet;
	L3INT li;
	L3INT sN = pIE->Size - sizeof(Q931ie_CallingNum);
	L3INT x;

	/* Octet 1 */
	OBuf[(*Octet)++] = Q931ie_CALLING_PARTY_NUMBER;
	
	/* Octet 2 */
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = 0x00 | (pIE->TypNum << 4) | (pIE->NumPlanID);
	
	/* Octet 4 */
	OBuf[(*Octet)++] = 0x80;

	/* Octet 5 */
	for (x = 0; x<sN; x++) {
		OBuf[(*Octet)++] = pIE->Digit[x];
	}

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_CallingSub

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_CallingSub(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_CallingSub *pie = (Q931ie_CallingSub*)OBuf;
	ie *pIE = &pMsg->CallingSub;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT x;
	L3INT IESize;

	*pIE = 0;

	/* Octet 1 */
	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	pie->TypNum     = (IBuf[Octet + Off] >> 4) & 0x07;
	pie->OddEvenInd = (IBuf[Octet + Off] >> 3) & 0x01;
	Octet++;
	
	/* Octet 4*/
	x = 0;
	do {
		pie->Digit[x] = IBuf[Octet + Off] & 0x7f;
		Off++;
		x++;
	} while (Q931MoreIE() && x < 20);

	Q931IESizeTest(Q931E_CALLINGSUB);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_CallingSub) + x -1;
	pie->Size = (L3UCHAR)(sizeof(Q931ie_CallingSub) + x -1);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_CallingSub

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_CallingSub(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_CallingSub *pIE = (Q931ie_CallingSub*)IBuf;
	L3INT rc = 0;
	L3INT Beg = *Octet;
	L3INT li;
	L3INT sN = pIE->Size - sizeof(Q931ie_CallingSub) + 1;
	L3INT x;

	/* Octet 1 */
	OBuf[(*Octet)++] = Q931ie_CALLING_PARTY_SUBADDRESS;
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = 0x80 | (pIE->TypNum << 4) | (pIE->OddEvenInd << 3);
	
	/* Octet 4 */
	for (x = 0; x<sN; x++) {
		OBuf[(*Octet)++] = pIE->Digit[x];
	}

	OBuf[(*Octet) - 1] |= 0x80; /* Terminate bit */

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:		Q931Uie_Cause
  
  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_Cause(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff) 
{
	Q931ie_Cause *pie = (Q931ie_Cause*)OBuf;
	ie *pIE = &pMsg->Cause;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];

	/* Octet 2*/
	IESize = IBuf[Octet++]; 

	/* Octet 3*/
	pie->CodStand = (IBuf[Octet + Off]>>5) & 0x03;
	pie->Location =  IBuf[Octet + Off] & 0x0f;

	/* Octet 3a */
	if ((IBuf[Octet + Off] & 0x80) == 0) {
		Off++;
		pie->Recom = IBuf[Octet + Off] & 0x7f;
	}
	Octet++;

	/* Octet 4 */
	pie->Value = IBuf[Octet + Off] & 0x7f;
	Octet++;

	/* Consume optional Diagnostic bytes */
	while (Q931MoreIE()) {
		Off++;
	};

	Q931IESizeTest(Q931E_CAUSE);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_Cause);
	pie->Size = sizeof(Q931ie_Cause);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:		Q931Pie_Cause

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_Cause(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_Cause *pIE = (Q931ie_Cause*)IBuf;
	L3INT rc = 0;
	L3INT Beg = *Octet;
	L3INT li;

	OBuf[(*Octet)++] = Q931ie_CAUSE;
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = 0x80 | (pIE->CodStand<<5) | pIE->Location;

	/* Octet 3a - currently not supported in send */

	/* Octet 4 */
	OBuf[(*Octet)++] = 0x80 | pIE->Value;

	/* Octet 5 - diagnostics not supported in send */

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_CongLevel
  
  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_CongLevel(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff) 
{
	Q931ie_CongLevel *pie = (Q931ie_CongLevel*)OBuf;
	ie *pIE = &pMsg->CongestionLevel;
	L3INT Off = 0;
	L3INT Octet = 0;

	*pIE = 0;

	pie->IEId      = IBuf[Octet] & 0xf0;
	pie->CongLevel = IBuf[Octet] & 0x0f;
	Octet ++;

	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_CongLevel);
	pie->Size = sizeof(Q931ie_CongLevel);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_CongLevel

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value: Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_CongLevel(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_CongLevel *pIE = (Q931ie_CongLevel*)IBuf;
	L3INT rc = 0;
	/* L3INT Beg = *Octet; */

	OBuf[(*Octet)++] = Q931ie_CONGESTION_LEVEL | pIE->CongLevel;

	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_ChanID

  Parameters:   IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Uie_ChanID(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR *OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_ChanID *pie = (Q931ie_ChanID*)OBuf;
	ie *pIE = &pMsg->ChanID;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;
//18 04 e1 80 83 01
	*pIE = 0;

	Q931Log(pTrunk, Q931_LOG_DEBUG, "Decoding ChanID IE\n");

	/* Octet 1 */
	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize    = IBuf[Octet++]; 

	/* Octet 3 */
	pie->IntIDPresent = (IBuf[Octet] >> 6) & 0x01;
	pie->IntType      = (IBuf[Octet] >> 5) & 0x01;
	pie->PrefExcl     = (IBuf[Octet] >> 3) & 0x01;
	pie->DChanInd     = (IBuf[Octet] >> 2) & 0x01;
	pie->InfoChanSel  =  IBuf[Octet] & 0x03;

	Off = Q931ReadExt(&IBuf[Octet++], Off);

	/* Octet 3.1 */
	if (pie->IntIDPresent) {
		pie->InterfaceID = IBuf[Octet + Off] & 0x7f;

		/* Temp fix. Interface id can be extended using the extension bit */
		/* this will read the octets, but do nothing with them. this is done */
		/* because the usage of this field is a little unclear */
		/* 30.jan.2001/JVB */
		Off = Q931ReadExt(&IBuf[Octet + Off], Off);
		Off++;
	}

	if ((Octet + Off - 2) != IESize) {
		/* Octet 3.2 */
		if (pie->IntType == 1) {        /* PRI etc */
			pie->CodStand    = (IBuf[Octet + Off] >> 5) & 0x03;
			pie->NumMap      = (IBuf[Octet + Off] >> 4) & 0x01;
			pie->ChanMapType =  IBuf[Octet + Off] & 0x0f;
			Off++;

			/* Octet 3.3 */
			/* Temp fix. Assume B channel. H channels not supported */
			pie->ChanSlot = IBuf[Octet + Off] & 0x7f;

			/* Some dialects don't follow the extension coding properly for this, but this should be safe for all */
			if ((Octet + Off - 1) != IESize) {
				Off = Q931ReadExt(&IBuf[Octet + Off], Off);
			}
			Off++;
		}
	}

	Q931IESizeTest(Q931E_CHANID);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_ChanID);
	pie->Size = sizeof(Q931ie_ChanID);

	if (pTrunk->loglevel == Q931_LOG_DEBUG) {
		const char *iface;
		char tmp[100] = "";

		if (!pie->IntType) {
			switch (pie->InfoChanSel) {
			case 0x0:
				iface = "None";
				break;
			case 0x1:
				iface = "B1";
				break;
			case 0x2:
				iface = "B2";
				break;
			default:
				iface = "Any Channel";
			}

			snprintf(tmp, sizeof(tmp)-1, "InfoChanSel: %d (%s)", pie->InfoChanSel, iface);
		}

		Q931Log(pTrunk, Q931_LOG_DEBUG,
			"\n-------------------------- Q.931 Channel ID ------------------------\n"
			"    Pref/Excl: %s, Interface Type: %s\n"
			"    %s\n"
			"--------------------------------------------------------------------\n\n",
			((pie->PrefExcl) ? "Preferred" : "Exclusive"),
			((pie->IntType) ? "PRI/Other" : "BRI"),
			tmp);
	}
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_ChanID

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_ChanID(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_ChanID *pIE = (Q931ie_ChanID*)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Beg = *Octet;	/* remember current offset */
	L3INT li;

	OBuf[(*Octet)++] = Q931ie_CHANNEL_IDENTIFICATION;
	li = (*Octet)++; /* remember length position */

	/* Octet 3 flags & BRI chan # */
	OBuf[(*Octet)++] = 0x80 
			| ((pIE->IntIDPresent << 6) & 0x40)
			| ((pIE->IntType << 5) & 0x20)
			| ((pIE->PrefExcl << 3) & 0x08)
			|  (pIE->InfoChanSel & 0x03);

	/* Octet 3.1 - Interface Identifier */
	if (pIE->IntIDPresent) {
		OBuf[(*Octet)++] = 0x80 | (pIE->InterfaceID & 0x7f);
	}

	/* Octet 3.2 & 3.3 - PRI */
	if (pIE->IntType) {
		OBuf[(*Octet)++]  = 0x80
			| ((pIE->CodStand << 5) & 0x60)
			| ((pIE->NumMap << 4) & 0x10)
			|  (pIE->ChanMapType & 0x0f);		/* TODO: support all possible channel map types */

		/* Octet 3.3 Channel number */
		switch (pIE->ChanMapType) {
		case 0x6:	/* Slot map: H0 Channel Units */	/* unsupported, Octets 3.3.1 - 3.3.3 */
			return Q931E_CHANID;

		case 0x8:	/* Slot map: H11 Channel Units */
		case 0x9:	/* Slot map: H12 Channel Units */
		default:	/* Channel number */
			OBuf[(*Octet)++] = 0x80 | (pIE->ChanSlot & 0x7f);
			break;
		}
	}

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}


/*****************************************************************************

  Function:	 Q931Uie_CRV

  Description:  Reading CRV. 

				The CRV is currently returned in the return value that 
				Q921Rx23 will assign to the CRV field in the unpacked
				message. CRV is basically 2 bytes etc, but the spec allows
				the use of longer CRV values.
  
  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: CRV

*****************************************************************************/
L3USHORT Q931Uie_CRV(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *IOff, L3INT *OOff)
{
	L3USHORT CRV = 0;
	L3INT Octet = *IOff;
	L3INT l = IBuf[Octet++]; 

	if (l == 1) {	/* One octet CRV */
		CRV = IBuf[Octet++] & 0x7F;
	}
	else if (l == 2) {	/* two octet CRV */
		CRV  = (IBuf[Octet++] & 0x7f) << 8;
		CRV |=  IBuf[Octet++];
	}
	else {
		/* Long CRV is not used, so we skip this */
		/* TODO: is it right to set to 0 here? */
		CRV = 0;
		Octet += l;
	}

	*IOff = Octet;
	return CRV;
}

/*****************************************************************************

  Function:	 Q931Uie_DateTime

  Parameters:   pTrunk		[IN]		Ptr to trunk information.
				pIE			[OUT]       ptr to Information Element id.
				IBuf		[IN]		ptr to a packed ie.
				OBuf		[OUT]	   ptr to buffer for Unpacked ie.
				IOff		[IN\OUT]	Input buffer offset
				OOff		[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_DateTime(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_DateTime * pie = (Q931ie_DateTime*)OBuf;
	ie *pIE = &pMsg->DateTime;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize = 0;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];
	
	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	/* Octet 3 - Year */
	pie->Year = IBuf[Octet++];

	/* Octet 4 - Month */
	pie->Month = IBuf[Octet++];

	/* Octet 5 - Day */
	pie->Day = IBuf[Octet++];

	/*******************************************************************
		The remaining part of the IE are optioinal, but only the length 
		can now tell us wherever these fields are present or not
		(always remember: IESize does not include ID and Size octet)
	********************************************************************/
	pie->Format = 0;

	/* Octet 6 - Hour (optional)*/
	if (IESize >= 4) {
		pie->Format = 1;
		pie->Hour = IBuf[Octet++];

		/* Octet 7 - Minute (optional)*/
		if (IESize >= 5) {
			pie->Format = 2;
			pie->Minute = IBuf[Octet++];

			/* Octet 8 - Second (optional)*/
			if (IESize >= 6) {
				pie->Format = 3;
				pie->Second = IBuf[Octet++];
			}
		}
	}

	Q931IESizeTest(Q931E_DATETIME);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_DateTime);
	pie->Size = sizeof(Q931ie_DateTime);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_DateTime

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_DateTime(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_DateTime *pIE = (Q931ie_DateTime*)IBuf;
	L3INT rc = 0;
	L3INT Beg = *Octet;
	L3INT li;

	OBuf[(*Octet)++] = Q931ie_DATETIME;
	li = (*Octet)++;

	OBuf[(*Octet)++] = pIE->Year;
	OBuf[(*Octet)++] = pIE->Month;
	OBuf[(*Octet)++] = pIE->Day;
	if (pIE->Format >= 1) {
		OBuf[(*Octet)++] = pIE->Hour;

		if (pIE->Format >= 2) {
			OBuf[(*Octet)++] = pIE->Minute;

			if (pIE->Format >= 3) {
				OBuf[(*Octet)++] = pIE->Second;
			}
		}
	}

	OBuf[li] = (L3UCHAR)((*Octet)-Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_Display

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_Display(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_Display *pie = (Q931ie_Display*)OBuf;
	ie *pIE = &pMsg->Display;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;
	L3INT x;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];
	IESize    = IBuf[Octet++]; 

	for (x = 0; x<IESize; x++) {
		pie->Display[x] = IBuf[Octet + Off] & 0x7f;
		Off++;
	}

	Q931IESizeTest(Q931E_DISPLAY);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_Display) + x - 1;
	pie->Size = (L3UCHAR)(sizeof(Q931ie_Display) + x - 1);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_Display

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_Display(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_Display *pIE = (Q931ie_Display*)IBuf;
	L3INT rc = 0;
	L3INT Beg = *Octet;
	L3INT li;
	L3INT DSize;
	L3INT x;

	OBuf[(*Octet)++] = Q931ie_DISPLAY;
	li = (*Octet)++;

	DSize = pIE->Size - sizeof(Q931ie_Display);

	for (x = 0; x< DSize; x++) {
		
		OBuf[(*Octet)++] = pIE->Display[x];
	}

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_HLComp

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_HLComp(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff) 
{
	Q931ie_HLComp * pie = (Q931ie_HLComp*)OBuf;
	ie *pIE = &pMsg->HLComp;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];

	/* Octet */
	IESize = IBuf[Octet++]; 

	/* Octet 3*/
	pie->CodStand  = (IBuf[Octet + Off] >>5) & 0x03;
	pie->Interpret = (IBuf[Octet + Off] >>2) & 0x07;
	pie->PresMeth  =  IBuf[Octet + Off] & 0x03;
	Octet++;

	/* Octet 4 */
	pie->HLCharID = IBuf[Octet + Off] & 0x7f;
	Octet++;
	
	/* Octet 4a*/
	if ((IBuf[Octet + Off - 1] & 0x80) == 0 && Q931MoreIE()) {
		if (pie->HLCharID == 0x5e || pie->HLCharID == 0x5f) {
			pie->EHLCharID = IBuf[Octet + Off] & 0x7f;
			Off++;
		}
		else if ( pie->HLCharID >= 0xc3 && pie->HLCharID <= 0xcf) {
			pie->EVideoTlfCharID = IBuf[Octet + Off] & 0x7f;
			Off++;
		}
		else {
			/* error Octet 4a indicated, but invalid value in Octet 4. */
			Q931SetError(pTrunk,Q931E_HLCOMP, 4, Off);
			return Q931E_HLCOMP;
		}
		Off = Q931ReadExt(&IBuf[Octet + Off], Off);
	}

	Q931IESizeTest(Q931E_HLCOMP);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_HLComp);
	pie->Size = sizeof(Q931ie_HLComp);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_HLComp

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_HLComp(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_HLComp *pIE = (Q931ie_HLComp*)IBuf;
	L3INT rc = 0;
	L3INT Beg = *Octet;
	L3INT li;

	OBuf[(*Octet)++] = Q931ie_HIGH_LAYER_COMPATIBILITY;
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = 0x80 | ((pIE->CodStand << 5) & 0x60) | ((pIE->Interpret << 2) & 0x1c) | (pIE->PresMeth & 0x03);

	/* Octet 4 */
	OBuf[(*Octet)++] = pIE->HLCharID;

	/* Octet 4a */
	if (pIE->HLCharID == 0x5e || pIE->HLCharID == 0x5f) {
		OBuf[(*Octet)++] = 0x80 | (pIE->EHLCharID & 0x7f);
	}
	else if ( pIE->HLCharID >= 0xc3 && pIE->HLCharID <= 0xcf) {
		OBuf[(*Octet)++] = 0x80 | (pIE->EVideoTlfCharID & 0x7f);
	}
	else {
		OBuf[(*Octet) - 1] |= 0x80;
	}

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_KeypadFac

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_KeypadFac(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_KeypadFac *pie = (Q931ie_KeypadFac*)OBuf;
	ie *pIE = &pMsg->KeypadFac;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;
	L3INT x;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];
	IESize = IBuf[Octet++]; 

	for (x = 0; x<IESize; x++) {
		pie->KeypadFac[x] = IBuf[Octet + Off] & 0x7f;
		Off++;
	}

	Q931IESizeTest(Q931E_KEYPADFAC);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_KeypadFac) + x - 1;
	pie->Size = (L3UCHAR)(sizeof(Q931ie_KeypadFac) + x - 1);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_KeypadFac

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_KeypadFac(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_KeypadFac *pIE = (Q931ie_KeypadFac*)IBuf;
	L3INT rc = 0;
	L3INT Beg = *Octet;
	L3INT li;
	L3INT DSize;
	L3INT x;

	OBuf[(*Octet)++] = Q931ie_KEYPAD_FACILITY;
	li = (*Octet)++;

	DSize = pIE->Size - sizeof(Q931ie_KeypadFac) + 1;

	for (x = 0; x< DSize; x++) {
		OBuf[(*Octet)++] = pIE->KeypadFac[x];
	}

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_LLComp

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_LLComp(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_LLComp *pie = (Q931ie_LLComp*)OBuf;
	ie *pIE = &pMsg->LLComp;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	pie->CodStand  = (IBuf[Octet + Off] >> 5) & 0x03;
	pie->ITransCap =  IBuf[Octet + Off] & 0x1f;
	Octet++;

	/* Octet 3a*/
	if (IsQ931Ext(IBuf[Octet + Off - 1])) {
		pie->NegotInd = (IBuf[Octet + Off] >> 6) & 0x01;
		Off++;
	}

	/* Octet 4 */
	pie->TransMode = (IBuf[Octet + Off] >> 5) & 0x03;
	pie->InfoRate  =  IBuf[Octet + Off] & 0x1f;

	Octet++;

	/* Octet 4.1 */
	if (pie->InfoRate == 0x14) { /* Mutirate */
		pie->RateMul = IBuf[Octet + Off] & 0x7f;
		Off++;
	}

	/* Octet 5 - Layer 1 Ident */
	if ((IBuf[Octet + Off] & 0x60) == 0x20) { /* Layer 1 Ident ? */
		pie->Layer1Ident = (IBuf[Octet + Off] >> 5) & 0x03;
		pie->UIL1Prot    =  IBuf[Octet + Off] & 0x1f;
		Octet++;

		/* Octet 5a */
		if (IsQ931Ext(IBuf[Octet + Off - 1])) {
			pie->SyncAsync = (IBuf[Octet + Off] >> 6) & 0x01;
			pie->Negot     = (IBuf[Octet + Off] >> 5) & 0x01;
			pie->UserRate  =  IBuf[Octet + Off] & 0x1f;
			Off++;

			/* Octet 5b - 2 options */
			if (IsQ931Ext(IBuf[Octet + Off - 1])) {
				if (pie->UIL1Prot == 0x01) { /* V.110, I.460 and X.30*/
					pie->InterRate = (IBuf[Octet + Off] >> 5) & 0x03;
					pie->NIConTx   = (IBuf[Octet + Off] >> 4) & 0x01;
					pie->NIConRx   = (IBuf[Octet + Off] >> 3) & 0x01;
					pie->FlowCtlTx = (IBuf[Octet + Off] >> 2) & 0x01;
					pie->FlowCtlRx = (IBuf[Octet + Off] >> 1) & 0x01;
					Off++;
				}
				else if (pie->UIL1Prot == 0x80) { /* V.120 */
					pie->HDR        = (IBuf[Octet + Off] >> 6) & 0x01;
					pie->MultiFrame = (IBuf[Octet + Off] >> 5) & 0x01;
					pie->ModeL1     = (IBuf[Octet + Off] >> 4) & 0x01;
					pie->NegotLLI   = (IBuf[Octet + Off] >> 3) & 0x01;
					pie->Assignor   = (IBuf[Octet + Off] >> 2) & 0x01;
					pie->InBandNeg  = (IBuf[Octet + Off] >> 1) & 0x01;
					Off++;
				}
				else if (pie->UIL1Prot == 0x07) { /* non standard */
					Off = Q931ReadExt(&IBuf[Octet + Off], Off);
					Off++;
				}
				else {
					Q931SetError(pTrunk,Q931E_LLCOMP, 5,2);
					return Q931E_LLCOMP;
				}

				/* Octet 5c */
				if (IsQ931Ext(IBuf[Octet + Off - 1])) {
					pie->NumStopBits = (IBuf[Octet + Off] >> 5) & 0x03;
					pie->NumDataBits = (IBuf[Octet + Off] >> 3) & 0x03;
					pie->Parity      =  IBuf[Octet + Off] & 0x07;
					Off++;

					/* Octet 5d */
					if (IsQ931Ext(IBuf[Octet + Off - 1])) {
						pie->DuplexMode	= (IBuf[Octet + Off] >> 6) & 0x01;
						pie->ModemType  =  IBuf[Octet + Off] & 0x3f;
						Off = Q931ReadExt(&IBuf[Octet + Off], Off);
						Off++;
					}
				}
			}
		}
	}

	/* Octet 6 - Layer 2 Ident */
	if ((IBuf[Octet + Off] & 0x60) == 0x40) { /* Layer 1 Ident ? */
		pie->Layer2Ident = (IBuf[Octet + Off] >>5) & 0x03;
		pie->UIL2Prot    =  IBuf[Octet + Off] & 0x1f;
		Octet++;

		/* Octet 6a */
		if (IsQ931Ext(IBuf[Octet + Off - 1])) {
			if (pie->UIL2Prot == 0x10) { /* 2nd 6a */
				pie->UsrSpcL2Prot = IBuf[Octet + Off] & 0x7f;
				Off++;
			}
			else { /* assume 1st 6a */
				pie->ModeL2  = (IBuf[Octet + Off] >> 5) & 0x03;
				pie->Q933use =  IBuf[Octet + Off] & 0x03;
				Off++;
			}
			/* Octet 6b */
			if (IsQ931Ext(IBuf[Octet + Off - 1])) {
				pie->WindowSize = IBuf[Octet + Off] & 0x7f;
				Off++;
			}
		}
	}

	/* Octet 7 - layer 3 Ident */
	if ((IBuf[Octet + Off] & 0x60) == 0x60) { /* Layer 3 Ident ? */
		pie->Layer3Ident = (IBuf[Octet + Off] >> 5) & 0x03;
		pie->UIL3Prot    =  IBuf[Octet + Off] & 0x1f;
		Octet++;

		/* Octet 7a */
		if (IsQ931Ext(IBuf[Octet + Off - 1])) {
			if (pie->UIL3Prot == 0x0b) {
				/* Octet 7a + 7b AddL3Info */
				pie->AddL3Info = ((IBuf[Octet + Off] << 4) & 0xf0)
								| (IBuf[Octet + Off + 1] & 0x0f);
				Off += 2;
			}
			else {
				if (pie->UIL3Prot == 0x1f) {
					pie->ModeL3 = (IBuf[Octet + Off] >> 5) & 0x03;
					Off++;
				}
				else {
					pie->OptL3Info = IBuf[Octet + Off] & 0x7f;
					Off++;
				}

				/* Octet 7b*/
				if (IsQ931Ext(IBuf[Octet + Off - 1])) {
					pie->DefPackSize = IBuf[Octet + Off] & 0x0f;
					Off++;

					/* Octet 7c */
					if (IsQ931Ext(IBuf[Octet + Off - 1])) {
						pie->PackWinSize= IBuf[Octet + Off] & 0x7f;
					}
				}
			}
		}
	}

	Q931IESizeTest(Q931E_LLCOMP);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_LLComp);
	pie->Size = sizeof(Q931ie_LLComp);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_LLComp

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value: Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_LLComp(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_LLComp *pIE = (Q931ie_LLComp*)IBuf;
	L3INT rc = 0;
	L3INT Beg = *Octet;
	L3INT li;

	OBuf[(*Octet)++] = Q931ie_LOW_LAYER_COMPATIBILITY;
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = (pIE->CodStand << 6) | pIE->ITransCap;

	/* Octet 3a */
	OBuf[(*Octet)++] = 0x80 | (pIE->NegotInd << 6);

	/* Octet 4 */
	OBuf[(*Octet)++] = 0x80 | (pIE->TransMode << 5) | pIE->InfoRate;

	/* Octet 4.1 */
	if (pIE->InfoRate == 0x18) {
		OBuf[(*Octet)++] = 0x80 | pIE->RateMul;
	}

	/* Octet 5 */
	if (pIE->Layer1Ident == 0x01) {
		OBuf[(*Octet)++] = (pIE->Layer1Ident << 5) | pIE->UIL1Prot;
		
		/* Octet 5a */
		if ((pIE->ITransCap == 0x08 && (pIE->UIL1Prot == 0x01 || pIE->UIL1Prot == 0x08))
			|| (pIE->ITransCap == 0x10 && (pIE->UIL1Prot == 0x02 || pIE->UIL1Prot == 0x03))) {
			OBuf[(*Octet)++] = (pIE->SyncAsync<<6) | (pIE->Negot<<5) | pIE->UserRate;
			
			/* Octet 5b*/
			if (pIE->UIL1Prot == 0x01) {
				OBuf[(*Octet)++] =  (pIE->InterRate << 5)
							| (pIE->NIConTx << 4)
							| (pIE->NIConTx << 3)
							| (pIE->FlowCtlTx << 2)
							| (pIE->FlowCtlRx << 1);
			}
			else if (pIE->UIL1Prot == 0x08) {
				OBuf[(*Octet)++] =  (pIE->HDR << 6)
							| (pIE->MultiFrame << 5)
							| (pIE->ModeL1 << 4)
							| (pIE->NegotLLI << 3)
							| (pIE->Assignor << 2)
							| (pIE->InBandNeg << 1);
			}
			else {
				OBuf[(*Octet) - 1] |= 0x80;
			}

			/* How to detect wherever 5c and 5d is to present is not clear
			 * but they have been inculded as 'standard'
			 * Octet 5c
			 */
			if (pIE->UIL1Prot == 0x01 || pIE->UIL1Prot == 0x08) {
				OBuf[(*Octet)++] = (pIE->NumStopBits << 5) | (pIE->NumDataBits << 3) | pIE->Parity ;

				/* Octet 5d */
				OBuf[(*Octet)++] = 0x80 | (pIE->DuplexMode << 6) | pIE->ModemType;
			}
		}
		else {
			OBuf[(*Octet) - 1] |= 0x80;
		}
	}

	/* Octet 6 */
	if (pIE->Layer2Ident == 0x02) {
		OBuf[(*Octet)++] = (pIE->Layer2Ident << 5) | pIE->UIL2Prot;

		/* Octet 6a*/
		if (pIE->UIL2Prot == 0x02 /* Q.921/I.441 */
		|| pIE->UIL2Prot == 0x06 /* X.25 link layer */
		|| pIE->UIL2Prot == 0x07 /* X.25 multilink */
		|| pIE->UIL2Prot == 0x09 /* HDLC ARM */
		|| pIE->UIL2Prot == 0x0a /* HDLC NRM */
		|| pIE->UIL2Prot == 0x0b /* HDLC ABM */
		|| pIE->UIL2Prot == 0x0d /* X.75 SLP */
		|| pIE->UIL2Prot == 0x0e /* Q.922 */
		|| pIE->UIL2Prot == 0x11) { /* ISO/ECE 7776 DTE-DCE */
			OBuf[(*Octet)++] = (pIE->ModeL2 << 5) | pIE->Q933use;

			/* Octet 6b */
			OBuf[(*Octet)++] = 0x80 | pIE->WindowSize;
		}
		else if (pIE->UIL2Prot == 0x10) { /* User Specific */
			OBuf[(*Octet)++] = 0x80 | pIE->UsrSpcL2Prot;
		}
		else {
			OBuf[(*Octet) - 1] |= 0x80;
		}
	}

	/* Octet 7 */
	if (pIE->Layer3Ident == 0x03) {
		OBuf[(*Octet)++] = (pIE->Layer3Ident << 5) | pIE->UIL3Prot;

		/* Octet 7a - 3 different ones */
		if (pIE->UIL3Prot == 0x10) {
			OBuf[(*Octet++)] = 0x80 | pIE->OptL3Info;
		}
		else if (pIE->UIL3Prot == 0x06
			||  pIE->UIL3Prot == 0x07
			||  pIE->UIL3Prot == 0x08) {
			OBuf[(*Octet)++] = pIE->ModeL3 << 5;

			/* Octet 7b note 7 */
			OBuf[(*Octet)++] = pIE->DefPackSize;

			/* Octet 7c note 7 */
			OBuf[(*Octet)++] = 0x80 | pIE->PackWinSize;
		}
		else if (pIE->UIL3Prot == 0x0b) {
			OBuf[(*Octet)++] = (pIE->AddL3Info >> 4) & 0x0f;
			OBuf[(*Octet)++] = 0x80 | (pIE->AddL3Info & 0x0f);
		}
		else {
			OBuf[(*Octet) - 1] |= 0x80;
		}
	}
	else {
		Q931SetError(pTrunk,Q931E_LLCOMP, 7,0);
		rc = Q931E_LLCOMP;
	}

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_NetFac

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_NetFac(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_NetFac *pie = (Q931ie_NetFac*)OBuf;
	ie *pIE = &pMsg->NetFac;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT x = 0;
	L3INT IESize;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	pie->LenNetID = IBuf[Octet + Off]; /* full octet is used */
	Octet++;

	if (pie->LenNetID > 0) {
		/* Octet 3.1 */
		pie->TypeNetID = (IBuf[Octet + Off] >> 4) & 0x0f;
		pie->NetIDPlan =  IBuf[Octet + Off] & 0x0f;
		Off = Q931ReadExt(&IBuf[Octet], Off);
		Off++;

		/* Octet 3.2*/
		for (x = 0; x < pie->LenNetID; x++) {
			pie->NetID[x] = IBuf[Octet + Off] & 0x7f;
			Off++;
		}
	}

	/* Octet 4*/
	pie->NetFac = IBuf[Octet + Off]; /* Full Octet is used */
	Octet++;

	Q931IESizeTest(Q931E_NETFAC);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_NetFac) + x - 1;
	pie->Size = (L3UCHAR)(sizeof(Q931ie_NetFac) + x - 1);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_NetFac

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_NetFac(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_NetFac *pIE = (Q931ie_NetFac*)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Beg = *Octet;
	L3INT li;
	L3INT x;

	OBuf[(*Octet)++] = Q931ie_NETWORK_SPECIFIC_FACILITIES;
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = pIE->LenNetID;

	if (pIE->LenNetID > 0) {
		/* Octet 3.1 */
		OBuf[(*Octet)++] = 0x80 | (pIE->TypeNetID << 4) | pIE->NetIDPlan;

		/* Octet 3.2 */
		for (x = 0; x <pIE->LenNetID; x++) {
			OBuf[(*Octet)++] = pIE->NetID[x];
		}
	}

	/* Octet 4 */
	OBuf[(*Octet)++] = pIE->NetFac;

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:		Q931Uie_NotifInd

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_NotifInd(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_NotifInd *pie = (Q931ie_NotifInd*)OBuf;
	ie *pIE = &pMsg->NotifInd;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];

	/* Octet 2*/
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	pie->Notification = IBuf[Octet + Off] & 0x7f;

	Off = Q931ReadExt(&IBuf[Octet], Off);
	Octet++;

	Q931IESizeTest(Q931E_NOTIFIND);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_NotifInd);
	pie->Size = sizeof(Q931ie_NotifInd);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_NotifInd

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_NotifInd(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_NotifInd *pIE = (Q931ie_NotifInd*)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Beg = *Octet;
	L3INT li;

	OBuf[(*Octet)++] = Q931ie_NOTIFICATION_INDICATOR;
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = pIE->Notification;

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_ProgInd

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_ProgInd(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_ProgInd *pie = (Q931ie_ProgInd*)OBuf;
	ie *pIE = &pMsg->ProgInd;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	pie->CodStand = (IBuf[Octet + Off] >> 5) & 0x03;
	pie->Location =  IBuf[Octet + Off] & 0x0f;

	Off = Q931ReadExt(&IBuf[Octet], Off);
	Octet++;

	/* Octet 4 */
	pie->ProgDesc = IBuf[Octet + Off] & 0x7f;
	Off = Q931ReadExt(&IBuf[Octet], Off);
	Octet++;

	Q931IESizeTest(Q931E_PROGIND);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_ProgInd);
	pie->Size = sizeof(Q931ie_ProgInd);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_ProgInd

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]	   Ptr tp packed output buffer.
				Octet[IN/OUT]   Offset L3INTo OBuf.

  Return Value: Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_ProgInd(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_ProgInd *pIE = (Q931ie_ProgInd*)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Beg = *Octet;
	L3INT li;

	OBuf[(*Octet)++] = Q931ie_PROGRESS_INDICATOR;
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = 0x80 | (pIE->CodStand << 5) | pIE->Location;

	/* Octet 4 */
	OBuf[(*Octet)++] = 0x80 | pIE->ProgDesc;

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_RepeatInd

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_RepeatInd(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_RepeatInd *pie = (Q931ie_RepeatInd*)OBuf;
	ie *pIE = &pMsg->RepeatInd;
	L3INT Off = 0;
	L3INT Octet = 0;

	*pIE = 0;

	pie->IEId      = IBuf[Octet] & 0xf0;
	pie->RepeatInd = IBuf[Octet] & 0x0f;
	Octet ++;

	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_RepeatInd);
	pie->Size = sizeof(Q931ie_RepeatInd);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_RepeatInd

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_RepeatInd(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_RepeatInd *pIE = (Q931ie_RepeatInd*)IBuf;
	L3INT rc = 0;
	/* L3INT Beg = *Octet; */

	OBuf[(*Octet)++] = Q931ie_REPEAT_INDICATOR | pIE->RepeatInd;

	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_RevChargeInd

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_RevChargeInd(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	ie iE;
	/* ie *pIE = &pMsg->RevChargeInd; */
	Q931SetIE(iE, *OOff);

	return iE;
}

/*****************************************************************************

  Function:	 Q931Pie_RevChargeInd

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_RevChargeInd(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	L3BOOL RetCode = L3FALSE;

	NoWarning(OBuf);
	NoWarning(IBuf);

	return RetCode;
}

/*****************************************************************************

  Function:	 Q931Uie_RestartInd

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_RestartInd(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_RestartInd *pie = (Q931ie_RestartInd*)OBuf;
	ie *pIE = &pMsg->RestartInd;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	pie->Class = IBuf[Octet + Off] & 0x07;
	pie->Spare = IBuf[Octet + Off] & 0x78;

	Off = Q931ReadExt(&IBuf[Octet], Off);
	Octet++;

	Q931IESizeTest(Q931E_RESTARTIND);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_RestartInd);
	pie->Size = sizeof(Q931ie_RestartInd);


	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_RestartInd

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_RestartInd(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_RestartInd *pIE = (Q931ie_RestartInd*)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Beg = *Octet;
	L3INT li;

	OBuf[(*Octet)++] = Q931ie_RESTART_INDICATOR;
	li = (*Octet)++;

	/* Octet 3*/
	OBuf[(*Octet)++] = 0x80 | pIE->Class ;

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_Segment

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_Segment(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_Segment *pie = (Q931ie_Segment*)OBuf;
	ie *pIE = &pMsg->Segment;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];
	Octet++;

	/* Octet 2*/
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	pie->FSI       = (IBuf[Octet + Off] & 0x80) >> 7;
	pie->NumSegRem =  IBuf[Octet + Off] & 0x7f;
	Octet++;

	/* Octet 4 */
	pie->SegType = IBuf[Octet + Off] & 0x7f;
	Octet++;
	
	Q931IESizeTest(Q931E_SEGMENT);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_Segment);
	pie->Size = sizeof(Q931ie_Segment);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_Segment

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_Segment(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_Segment *pIE = (Q931ie_Segment*)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Beg = *Octet;
	L3INT li;

	OBuf[(*Octet)++] = Q931ie_SEGMENTED_MESSAGE;
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = (pIE->FSI << 7) | pIE->NumSegRem;

	/* Octet 4 */
	OBuf[(*Octet)++] = pIE->SegType;

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:		Q931Uie_SendComplete

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_SendComplete(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_SendComplete *pie = (Q931ie_SendComplete*)OBuf;
	ie *pIE = &pMsg->SendComplete;
	L3INT Off = 0;
	L3INT Octet = 0;

	*pIE = 0;
	Octet++;

	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_SendComplete);
	pie->Size = sizeof(Q931ie_SendComplete);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_ProgInd

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]	   Ptr tp packed output buffer.
				Octet[IN/OUT]   Offset into OBuf.

  Return Value: Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_SendComplete(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	/* Q931ie_SendComplete * pIE = (Q931ie_SendComplete*)IBuf; */
	L3INT rc = Q931E_NO_ERROR;
	/* L3INT Beg = *Octet; */

	OBuf[(*Octet)++] = 0x80 | (L3UCHAR)Q931ie_SENDING_COMPLETE;

	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_Signal

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_Signal(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_Signal *pie = (Q931ie_Signal*)OBuf;
	ie *pIE = &pMsg->Signal;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	pie->Signal = IBuf[Octet + Off];
	Octet++;

	Q931IESizeTest(Q931E_SIGNAL);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_Signal);
	pie->Size = sizeof(Q931ie_Signal);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_Signal

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_Signal(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_Signal *pIE = (Q931ie_Signal*)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Beg = *Octet;
	L3INT li;

	OBuf[(*Octet)++] = Q931ie_SIGNAL;
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = pIE->Signal;

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_TransNetSel

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_TransNetSel(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_TransNetSel *pie = (Q931ie_TransNetSel*)OBuf;
	ie *pIE = &pMsg->TransNetSel;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT x = 0;
	L3INT l;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	l = IBuf[Octet++] - 3; 

	/* Octet 3 */
	pie->Type = (IBuf[Octet + Off] >> 4) & 0x07;

	Off = Q931ReadExt(&IBuf[Octet], Off);
	Octet++;

	for (x = 0; x < l; x++) {
		pie->NetID[x] = IBuf[Octet + Off] & 0x7f;
		Off++;
	}

	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_TransNetSel) + x - 1;
	pie->Size = (L3UCHAR)(sizeof(Q931ie_TransNetSel) + x - 1);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_TransNetSel

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_TransNetSel(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_TransNetSel *pIE = (Q931ie_TransNetSel*)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Beg = *Octet;
	L3INT li;
	L3INT x;
	L3INT l;

	OBuf[(*Octet)++] = Q931ie_TRANSIT_NETWORK_SELECTION;
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = 0x80 | (pIE->Type << 4) | pIE->NetIDPlan;

	/* Octet 4 */
	l = pIE->Size - sizeof(Q931ie_TransNetSel) + 1;
	for (x = 0; x < l; x++) {
		OBuf[(*Octet)++] = pIE->NetID[x];
	}

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_UserUser

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_UserUser(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_UserUser *pie = (Q931ie_UserUser*)OBuf;
	ie *pIE = &pMsg->UserUser;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT l;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	l = IBuf[Octet++] - 1;

	/* Octet 3 */
	pie->ProtDisc = IBuf[Octet++];

	for (Off = 0; Off < l; Off++) {
		pie->User[Off] = IBuf[Octet + Off];
	}

	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_UserUser) + Off - 1;
	pie->Size = (L3UCHAR)(sizeof(Q931ie_UserUser) + Off - 1);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_UserUser

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_UserUser(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_UserUser *pIE = (Q931ie_UserUser*)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Beg = *Octet;
	L3INT li;
	L3INT x;
	L3INT l;

	OBuf[(*Octet)++] = Q931ie_USER_USER;
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = pIE->ProtDisc;

	/* Octet 4 */
	l = pIE->Size - sizeof(Q931ie_UserUser) + 1;
	for (x = 0; x < l; x++) {
		OBuf[(*Octet)++] = pIE->User[x];
	}

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}

/*****************************************************************************

  Function:	 Q931Uie_GenericDigits

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset


				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_GenericDigits(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_GenericDigits *pie = (Q931ie_GenericDigits*)OBuf;
	ie *pIE = &pMsg->GenericDigits;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT x;
	L3INT IESize;

	*pIE = 0;

	/* Octet 1 */
	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	pie->Type     = (IBuf[Octet]) & 0x1F;
	pie->Encoding = (IBuf[Octet] >> 5) & 0x07;
	Octet++;
	
	/* Octet 4*/
	if (pie->Encoding == 0) { /* BCD Even */
		x = 0;
		do {
			pie->Digit[x++] =  IBuf[Octet + Off] & 0x0f;
			pie->Digit[x++] = (IBuf[Octet + Off] >> 4) & 0x0f;
			Off++;
		} while (Q931MoreIE());
	} else if (pie->Encoding == 1) { /* BCD Odd */
		x = 0;
		do {
			pie->Digit[x++] = IBuf[Octet + Off] & 0x0f;
			if (Q931MoreIE()) {
				pie->Digit[x] = (IBuf[Octet + Off] >> 4) & 0x0f;
			}
			x++;
			Off++;
		} while (Q931MoreIE());
	} else if (pie->Encoding == 2) { /* IA5 */
		x = 0;
		do {
			pie->Digit[x++] = IBuf[Octet + Off] & 0x7f;
			Off++;
		} while (Q931MoreIE());
	} else {
		/* Binary encoding type unkown */
		Q931SetError(pTrunk, Q931E_GENERIC_DIGITS, Octet, Off);
		return Q931E_GENERIC_DIGITS;
	}

	Q931IESizeTest(Q931E_GENERIC_DIGITS);
	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_CallingSub) + x - 1;
	pie->Size = (L3UCHAR)(sizeof(Q931ie_CallingSub) + x - 1);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_GenericDigits

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/

L3INT Q931Pie_GenericDigits(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	OBuf[(*Octet)++] = (Q931ie_GENERIC_DIGITS & 0xFF);
	OBuf[(*Octet)++] = 0;

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Uie_ChangeStatus

  Parameters:   pIE[OUT]		ptr to Information Element id.
				IBuf[IN]		ptr to a packed ie.
				OBuf[OUT]	   ptr to buffer for Unpacked ie.
				IOff[IN\OUT]	Input buffer offset
				OOff[IN\OUT]	Output buffer offset

				Ibuf and OBuf points directly to buffers. The IOff and OOff
				must be updated, but are otherwise not used in the ie unpack.

  Return Value: Error Message

*****************************************************************************/
L3INT Q931Uie_ChangeStatus(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	Q931ie_ChangeStatus *pie = (Q931ie_ChangeStatus*)OBuf;
	ie *pIE = &pMsg->ChangeStatus;
	L3INT Off = 0;
	L3INT Octet = 0;
	L3INT IESize;

	*pIE = 0;

	pie->IEId = IBuf[Octet++];

	/* Octet 2 */
	IESize = IBuf[Octet++]; 

	/* Octet 3 */
	pie->Preference = (IBuf[Octet + Off] >> 6) & 0x01;
	pie->Spare      =  IBuf[Octet + Off] & 0x38;
	pie->NewStatus  =  IBuf[Octet + Off] & 0x07;
	Octet++;

	Q931SetIE(*pIE, *OOff);

	*IOff = (*IOff) + Octet + Off;
	*OOff = (*OOff) + sizeof(Q931ie_ChangeStatus);
	pie->Size = sizeof(Q931ie_ChangeStatus);

	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pie_ChangeStatus

  Parameters:   IBuf[IN]		Ptr to struct.
				OBuf[OUT]		Ptr tp packed output buffer.
				Octet[IN/OUT]	Offset into OBuf.

  Return Value:	Error code, 0 = OK

*****************************************************************************/
L3INT Q931Pie_ChangeStatus(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	Q931ie_ChangeStatus *pIE = (Q931ie_ChangeStatus*)IBuf;
	L3INT rc = Q931E_NO_ERROR;
	L3INT Beg = *Octet;
	L3INT li;

	OBuf[(*Octet)++] = Q931ie_CHANGE_STATUS;
	li = (*Octet)++;

	/* Octet 3 */
	OBuf[(*Octet)++] = 0x80 | pIE->NewStatus | ((pIE->Preference & 0x01) << 6);

	OBuf[li] = (L3UCHAR)((*Octet) - Beg) - 2;
	return rc;
}



L3INT Q931Uie_Generic(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *IOff, L3INT *OOff)
{
	L3INT Octet = 0;
	L3UCHAR id = 0;

	/* id */
	id = IBuf[Octet++];

	/* Length */
	Octet += IBuf[Octet];
	Octet++;

	Q931Log(pTrunk, Q931_LOG_DEBUG, "Discarding IE %#hhx with length %d\n", id, Octet - 2);

	*IOff += Octet;
	return Q931E_NO_ERROR;
}

L3INT Q931Pie_Generic(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	/* do nothing */
	return Q931E_NO_ERROR;
}
