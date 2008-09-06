/*****************************************************************************

  FileName:		Q932.h

  Contents:		Header w/structs for Q932 Suplementary Services.

  NB:			Do NOT include this header directly, include Q931.h

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

/*****************************************************************************
  Q.932 Additional Message codes
*****************************************************************************/

#define Q932mes_HOLD                 0x24 /* 0010 0100                      */
#define Q932mes_HOLD_ACKNOWLEDGE     0x28 /* 0010 1000                      */
#define Q932mes_HOLD_REJECT          0x30 /* 0011 0000                      */
#define Q932mes_RETRIEVE             0x31 /* 0011 0001                      */
#define Q932mes_RETRIEVE_ACKNOWLEDGE 0x33 /* 0011 0011                      */
#define Q932mes_RETRIEVE_REJECT      0x37 /* 0011 0111                      */
#define Q932mes_FACILITY             0x62 /* 0110 0010                      */
#define Q932mes_REGISTER             0x64 /* 0110 0100                      */

/*****************************************************************************
  Q.932 Additional EI Codes
*****************************************************************************/
#define Q932ie_FACILITY              0x1c /* 0001 1100                      */

/*****************************************************************************
  Function Prototypes.
*****************************************************************************/
L3INT Q932ProcFacilityTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcHoldTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcHoldAckTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcHoldRejectTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcRegisterTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcRetrieveTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcRetrieveAckTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcRetrieveRejectTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);

L3INT Q932ProcFacilityNT(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcHoldNT(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcHoldAckNT(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcHoldRejectNT(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcRegisterNT(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcRetrieveNT(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcRetrieveAckNT(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);
L3INT Q932ProcRetrieveRejectNT(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT iFrom);

L3INT Q932Pmes_Facility(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q932Pmes_Hold(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q932Pmes_HoldAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q932Pmes_HoldReject(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q932Pmes_Register(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q932Pmes_Retrieve(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q932Pmes_RetrieveAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q932Pmes_RetrieveReject(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);

L3INT Q932Umes_Facility(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic * OBuf, L3INT I, L3INT O);
L3INT Q932Umes_Hold(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic * OBuf, L3INT I, L3INT O);
L3INT Q932Umes_HoldAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic * OBuf, L3INT I, L3INT O);
L3INT Q932Umes_HoldReject(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic * OBuf, L3INT I, L3INT O);
L3INT Q932Umes_Register(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic * OBuf, L3INT I, L3INT O);
L3INT Q932Umes_Retrieve(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic * OBuf, L3INT I, L3INT O);
L3INT Q932Umes_RetrieveAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic * OBuf, L3INT I, L3INT O);
L3INT Q932Umes_RetrieveReject(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic * OBuf, L3INT I, L3INT O);
