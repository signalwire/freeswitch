/*****************************************************************************

  FileName:	 Q931.c

  Contents:	 Implementation of Q.931 stack main interface functions. 
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

#include "Q921.h"
#include "Q931.h"
#include "national.h"
#include "DMS.h"
#include "5ESS.h"


/*****************************************************************************

  Dialect function pointers tables.

  The following function pointer arrays define pack/unpack functions and 
  processing furnctions for the different Q.931 based dialects.

  The arrays are initialized with pointers to dummy functions and later
  overrided with pointers to actual functions as new dialects are added.

  The initial Q.931 will as an example define 2 dialects as it treats User
  and Network mode as separate ISDN dialects.

  The API messages Q931AddProc, Q931AddMes, Q931AddIE are used to initialize
  these table entries during system inititialization of a stack.

*****************************************************************************/
q931proc_func_t *Q931Proc[Q931MAXDLCT][Q931MAXMES];

q931umes_func_t *Q931Umes[Q931MAXDLCT][Q931MAXMES];
q931pmes_func_t *Q931Pmes[Q931MAXDLCT][Q931MAXMES];

q931uie_func_t *Q931Uie[Q931MAXDLCT][Q931MAXIE];
q931pie_func_t *Q931Pie[Q931MAXDLCT][Q931MAXIE];

q931timeout_func_t *Q931Timeout[Q931MAXDLCT][Q931MAXTIMER];
q931timer_t         Q931Timer[Q931MAXDLCT][Q931MAXTIMER];

void (*Q931CreateDialectCB[Q931MAXDLCT])(L3UCHAR iDialect) = { NULL, NULL };

Q931State Q931st[Q931MAXSTATE];

/*****************************************************************************

  Core system tables and variables.

*****************************************************************************/

L3INT Q931L4HeaderSpace = {0};	/* header space to be ignoder/inserted */
				/* at head of each message. */

L3INT Q931L2HeaderSpace = {4};	/* Q921 header space, sapi, tei etc */

/*****************************************************************************

  Main interface callback functions. 

*****************************************************************************/

Q931ErrorCB_t Q931ErrorProc;			/* callback for error messages. */
L3ULONG (*Q931GetTimeProc) (void) = NULL;	/* callback for func reading time in ms */

/*****************************************************************************

  Function:	 Q931SetL4HeaderSpace

  Description:  Set the # of bytes to be inserted/ignored at the head of
		each message. Q931 will issue a message with space for header
		and the user will use this to fill in whatever header info
		is required to support the architecture used.

*****************************************************************************/
void Q931SetL4HeaderSpace(L3INT space)
{
	Q931L4HeaderSpace = space;
}

/*****************************************************************************

  Function:	 Q931SetL2HeaderSpace

  Description:  Set the # of bytes to be inserted/ignored at the head of
		each message. Q931 will issue a message with space for header
		and the user will use this to fill in whatever header info
		is required to support the architecture used.

*****************************************************************************/
void Q931SetL2HeaderSpace(L3INT space)
{
	Q931L2HeaderSpace = space;
}

/*****************************************************************************

  Function:	 Q931ProcDummy

  Description:  Dummy function for message processing.

*****************************************************************************/
L3INT Q931ProcDummy(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b,L3INT c)
{
	return Q931E_INTERNAL;
}

/*****************************************************************************

  Function:	 Q931UmesDummy

  Description:  Dummy function for message processing

*****************************************************************************/
L3INT Q931UmesDummy(Q931_TrunkInfo_t *pTrunk,L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT IOff, L3INT Size)
{
	return Q931E_UNKNOWN_MESSAGE;
}

/*****************************************************************************

  Function:	 Q931UieDummy

  Description:  Dummy function for message processing

*****************************************************************************/
L3INT Q931UieDummy(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
	return Q931E_UNKNOWN_IE;
}

/*****************************************************************************

  Function:	 Q931PmesDummy

  Description:  Dummy function for message processing

*****************************************************************************/
L3INT Q931PmesDummy(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	return Q931E_UNKNOWN_MESSAGE;
}

/*****************************************************************************

  Function:	 Q931PieDummy

  Description:  Dummy function for message processing

*****************************************************************************/
L3INT Q931PieDummy(Q931_TrunkInfo_t *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
	return Q931E_UNKNOWN_IE;
}

/*****************************************************************************

  Function:	 Q931TxDummy

  Description:  Dummy function for message processing

*****************************************************************************/
L3INT Q931TxDummy(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT n)
{
	return Q931E_MISSING_CB;
}

/*****************************************************************************

  Function:	 Q931ErrorDummy

  Description:  Dummy function for error processing

*****************************************************************************/
L3INT Q931ErrorDummy(void *priv, L3INT a, L3INT b, L3INT c)
{
	return 0;
}

/*****************************************************************************

  Function:	 Q931Initialize

  Description:  This function Initialize the stack. 
  
		Will set up the trunk array, channel
		arrays and initialize Q931 function arrays before it finally
		set up EuroISDN processing with User as dialect 0 and 
		Network as dialect 1.

  Note:		 Initialization of other stacks should be inserted after
		the initialization of EuroISDN.

*****************************************************************************/
void Q931Initialize()
{
	L3INT x,y;

	/* Secure the callbacks to default procs */
	Q931ErrorProc = Q931ErrorDummy;

	/* The user will only add the message handlers and IE handlers he need,
	 * so we need to initialize every single entry to a default function
	 * that will throw an appropriate error if they are ever called.
	 */
	for (x = 0; x < Q931MAXDLCT; x++) {
		for (y = 0; y < Q931MAXMES; y++) {
			Q931Proc[x][y] = Q931ProcDummy;
			Q931Umes[x][y] = Q931UmesDummy;
			Q931Pmes[x][y] = Q931PmesDummy;
		}
		for (y = 0; y < Q931MAXIE; y++) {
			Q931Pie[x][y] = Q931PieDummy;
			Q931Uie[x][y] = Q931UieDummy;
		}
		for (y = 0; y < Q931MAXTIMER; y++) {
			Q931Timeout[x][y] = Q931TimeoutDummy;
			Q931Timer[x][y]   = 0;
		}
	}

	if (Q931CreateDialectCB[Q931_Dialect_Q931 + Q931_TE] == NULL)
		Q931AddDialect(Q931_Dialect_Q931 + Q931_TE, Q931CreateTE);

	if (Q931CreateDialectCB[Q931_Dialect_Q931 + Q931_NT] == NULL)
		Q931AddDialect(Q931_Dialect_Q931 + Q931_NT, Q931CreateNT);

	if (Q931CreateDialectCB[Q931_Dialect_National + Q931_TE] == NULL)
		Q931AddDialect(Q931_Dialect_National + Q931_TE, nationalCreateTE);

	if (Q931CreateDialectCB[Q931_Dialect_National + Q931_NT] == NULL)
		Q931AddDialect(Q931_Dialect_National + Q931_NT, nationalCreateNT);

	if (Q931CreateDialectCB[Q931_Dialect_DMS + Q931_TE] == NULL)
		Q931AddDialect(Q931_Dialect_DMS + Q931_TE, DMSCreateTE);

	if (Q931CreateDialectCB[Q931_Dialect_DMS + Q931_NT] == NULL)
		Q931AddDialect(Q931_Dialect_DMS + Q931_NT, DMSCreateNT);

	if (Q931CreateDialectCB[Q931_Dialect_5ESS + Q931_TE] == NULL)
		Q931AddDialect(Q931_Dialect_5ESS + Q931_TE, ATT5ESSCreateTE);

	if (Q931CreateDialectCB[Q931_Dialect_5ESS + Q931_NT] == NULL)
		Q931AddDialect(Q931_Dialect_5ESS + Q931_NT, ATT5ESSCreateNT);

	/* The last step we do is to call the callbacks to create the dialects  */
	for (x = 0; x < Q931MAXDLCT; x++) {
		if (Q931CreateDialectCB[x] != NULL) {
			Q931CreateDialectCB[x]((L3UCHAR)x);
		}   
	}
}

/**
 * Q931TimerTick
 * \brief	Periodically called to update and check for expired timers
 * \param	pTrunk	Q.931 trunk
 */
void Q931TimerTick(Q931_TrunkInfo_t *pTrunk)
{
	struct Q931_Call *call = NULL;
	L3ULONG now = 0;
	L3INT x;

	/* TODO: Loop through all active calls, check timers and call timout procs
	 * if timers are expired.
	 * Implement a function array so each dialect can deal with their own
	 * timeouts.
	 */
	now = Q931GetTime();

	for (x = 0; x < Q931MAXCALLPERTRUNK; x++) {
		call = &pTrunk->call[x];

		if (!call->InUse || !call->Timer || !call->TimerID)
			continue;

		if (call->Timer <= now) {
			/* Stop Timer */
			Q931StopTimer(pTrunk, x, call->TimerID);

			/* Invoke dialect timeout callback */
			Q931Timeout[pTrunk->Dialect][call->TimerID](pTrunk, x);
		}
	}
}

/*****************************************************************************

  Function:	 Q931Rx23

  Description:  Receive message from layer 2 (LAPD). Receiving a message 
				is always done in 2 steps. First the message must be 
				interpreted and translated to a static struct. Secondly
				the message is processed and responded to.

				The Q.931 message contains a static header that is 
				interpreted in this function. The rest is interpreted
				in a sub function according to mestype.

  Parameters:   pTrunk  [IN]	Ptr to trunk info.
				buf	 [IN]	Ptr to buffer containing message.
				Size	[IN]	Size of message.

  Return Value: Error Code. 0 = No Error, < 0 :error, > 0 : Warning
				see q931errors.h for details.

*****************************************************************************/
L3INT Q931Rx23(Q931_TrunkInfo_t *pTrunk, L3INT ind, L3UCHAR tei, L3UCHAR * buf, L3INT Size)
{
	L3UCHAR *Mes = NULL;
	L3INT RetCode = Q931E_NO_ERROR;
	Q931mes_Generic *m = (Q931mes_Generic *) pTrunk->L3Buf;
	L3INT ISize;
	L3INT IOff = 0;
	L3INT L2HSize = Q931L2HeaderSpace;

	switch (ind) {
	case Q921_DL_UNIT_DATA:		/* DL-UNITDATA indication (UI frame, 3 byte header) */
		L2HSize = 3;

	case Q921_DL_DATA:		/* DL-DATA indication (I frame, 4 byte header) */
		/* Reset our decode buffer */
		memset(pTrunk->L3Buf, 0, sizeof(pTrunk->L3Buf));

		/* L2 Header Offset */
		Mes = &buf[L2HSize];

		/* Protocol Discriminator */
		m->ProtDisc = Mes[IOff++];

		/* CRV */
		m->CRVFlag = (Mes[IOff + 1] >> 7) & 0x01;
		m->CRV = Q931Uie_CRV(pTrunk, Mes, m->buf, &IOff, &ISize);

		/* Message Type */
		m->MesType = Mes[IOff++];

		/* Store tei */
		m->Tei = tei;

		/* d'oh a little ugly but this saves us from:
		 *	a) doing Q.921 work in the lower levels (extracting the TEI ourselves)
		 *	b) adding a tei parameter to _all_ Proc functions
		 */
		if (tei) {
			L3INT callIndex = 0;

			/* Find the call using CRV */
			RetCode = Q931FindCRV(pTrunk, m->CRV, &callIndex);
			if (RetCode == Q931E_NO_ERROR && !pTrunk->call[callIndex].Tei) {
				pTrunk->call[callIndex].Tei = tei;
			}
		}

		Q931Log(pTrunk, Q931_LOG_DEBUG, "Received message from Q.921 (ind %d, tei %d, size %d)\nMesType: %d, CRVFlag %d (%s), CRV %d (Dialect: %d)\n", ind, m->Tei, Size,
						 m->MesType, m->CRVFlag, m->CRVFlag ? "Terminator" : "Originator", m->CRV, pTrunk->Dialect);

		RetCode = Q931Umes[pTrunk->Dialect][m->MesType](pTrunk, Mes, (Q931mes_Generic *)pTrunk->L3Buf, IOff, Size - L2HSize);
		if (RetCode >= Q931E_NO_ERROR) {
			RetCode = Q931Proc[pTrunk->Dialect][m->MesType](pTrunk, pTrunk->L3Buf, 2);
		}
		break;

	default:
		break;
	}

	return RetCode;
}

/*****************************************************************************

  Function:	 Q931Tx34

  Description:  Called from the stack to send a message to layer 4.

  Parameters:   Mes[IN]	 Ptr to message buffer.
		Size[IN]	Message size in bytes.

  Return Value: Error Code. 0 = No Error, < 0 :error, > 0 : Warning
		see q931errors.h for details.

*****************************************************************************/
L3INT Q931Tx34(Q931_TrunkInfo_t *pTrunk, L3UCHAR * Mes, L3INT Size)
{
	Q931Log(pTrunk, Q931_LOG_DEBUG, "Sending message to Layer4 (size: %d)\n", Size);

	if (pTrunk->Q931Tx34CBProc) {
		return pTrunk->Q931Tx34CBProc(pTrunk->PrivateData34, Mes, Size);
	}
	return Q931E_MISSING_CB;	
}

/*****************************************************************************

  Function:	 Q931Rx43

  Description:  Receive message from Layer 4 (application).

  Parameters:   pTrunk[IN]  Trunk #.
		buf[IN]	 Message Pointer.
		Size[IN]	Message size in bytes.

  Return Value: Error Code. 0 = No Error, < 0 :error, > 0 : Warning
		see q931errors.h for details.

*****************************************************************************/
L3INT Q931Rx43(Q931_TrunkInfo_t *pTrunk,L3UCHAR * buf, L3INT Size)
{
	Q931mes_Header *ptr = (Q931mes_Header*)&buf[Q931L4HeaderSpace];
	L3INT RetCode = Q931E_NO_ERROR;

	Q931Log(pTrunk, Q931_LOG_DEBUG, "Receiving message from Layer4 (size: %d, type: %d)\n", Size, ptr->MesType);

	RetCode = Q931Proc[pTrunk->Dialect][ptr->MesType](pTrunk, buf, 4);

	Q931Log(pTrunk, Q931_LOG_DEBUG, "Q931Rx43 return code: %d\n", RetCode);

	return RetCode;
}

/*****************************************************************************

  Function:	 Q931Tx32

  Description:  Called from the stack to send a message to L2. The input is 
				always a non-packed message so it will first make a proper
				call to create a packed message before it transmits that
				message to layer 2.

  Parameters:   pTrunk[IN]  Trunk #  
				buf[IN]	 Ptr to message buffer.
				Size[IN]	Message size in bytes.

  Return Value: Error Code. 0 = No Error, < 0 :error, > 0 : Warning
				see q931errors.h for details.

*****************************************************************************/
L3INT Q931Tx32Data(Q931_TrunkInfo_t *pTrunk, L3UCHAR bcast, L3UCHAR * Mes, L3INT Size)
{
	Q931mes_Generic *ptr = (Q931mes_Generic*)Mes;
	L3INT RetCode = Q931E_NO_ERROR;
	L3INT iDialect = pTrunk->Dialect;
	L3INT Offset = bcast ? (Q931L2HeaderSpace - 1) : Q931L2HeaderSpace;
	L3INT OSize;

	Q931Log(pTrunk, Q931_LOG_DEBUG, "Sending message to Q.921 (size: %d)\n", Size);

	memset(pTrunk->L2Buf, 0, sizeof(pTrunk->L2Buf));

	/* Call pack function through table. */
	RetCode = Q931Pmes[iDialect][ptr->MesType](pTrunk, (Q931mes_Generic *)Mes, Size, &pTrunk->L2Buf[Offset], &OSize);
	if (RetCode >= Q931E_NO_ERROR) {
		L3INT callIndex;
		L3UCHAR tei = 0;

		if (ptr->CRV) {
			/* Find the call using CRV */
			RetCode = Q931FindCRV(pTrunk, ptr->CRV, &callIndex);
			if (RetCode != Q931E_NO_ERROR)
				return RetCode;

			tei = pTrunk->call[callIndex].Tei;
		}

		if (pTrunk->Q931Tx32CBProc) {
			RetCode = pTrunk->Q931Tx32CBProc(pTrunk->PrivateData32, bcast ? Q921_DL_UNIT_DATA : Q921_DL_DATA, tei, pTrunk->L2Buf, OSize + Offset);
		} else {
			RetCode = Q931E_MISSING_CB;
		}
	}

	return RetCode;
}


/*****************************************************************************

  Function:	 Q931SetError

  Description:  Called from the stack to indicate an error. 

  Parameters:   ErrID	   ID of ie or message causing error.
		ErrPar1	 Error parameter 1
		ErrPar2	 Error parameter 2.


*****************************************************************************/
void Q931SetError(Q931_TrunkInfo_t *pTrunk,L3INT ErrID, L3INT ErrPar1, L3INT ErrPar2)
{
	if (pTrunk->Q931ErrorCBProc) {
		pTrunk->Q931ErrorCBProc(pTrunk->PrivateData34, ErrID, ErrPar1, ErrPar2);
	} else {
		Q931ErrorProc(pTrunk->PrivateData34, ErrID, ErrPar1, ErrPar2);
	}
}

void Q931SetDefaultErrorCB(Q931ErrorCB_t Q931ErrorPar)
{
	Q931ErrorProc = Q931ErrorPar;
}

/*****************************************************************************

  Function:	 Q931CreateCRV

  Description:  Create a CRV entry and return it's index. The function will 
		locate a free entry in the call tables allocate it and 
		allocate a unique CRV value attached to it.

  Parameters:   pTrunk	  [IN]	Trunk number
		callindex   [OUT]   return call table index.

  Return Value: Error Code. 0 = No Error, < 0 :error, > 0 : Warning
		see q931errors.h for details.
****************************************************************************/
L3INT Q931CreateCRV(Q931_TrunkInfo_t *pTrunk, L3INT * callIndex)
{
	L3INT CRV = Q931GetUniqueCRV(pTrunk);

	return Q931AllocateCRV(pTrunk, CRV, callIndex);
}


L3INT Q931ReleaseCRV(Q931_TrunkInfo_t *pTrunk, L3INT CRV)
{
	int callIndex;
	
	if ((Q931FindCRV(pTrunk, CRV, &callIndex)) == Q931E_NO_ERROR) {
		pTrunk->call[callIndex].InUse = 0;
		return Q931E_NO_ERROR;
	}

	return Q931E_INVALID_CRV;
}

/*****************************************************************************

  Function:	 Q931AllocateCRV

  Description:  Allocate a call table entry and assigns the given CRV value
		to it.

  Parameters:   pTrunk	  [IN]	Trunk number
		iCRV		[IN]	Call Reference Value.
		callindex   [OUT]   return call table index.

  Return Value: Error Code. 0 = No Error, < 0 :error, > 0 : Warning
		see q931errors.h for details.

*****************************************************************************/
L3INT Q931AllocateCRV(Q931_TrunkInfo_t *pTrunk, L3INT iCRV, L3INT * callIndex)
{
	L3INT x;
	for (x = 0; x < Q931MAXCALLPERTRUNK; x++) {
		if (!pTrunk->call[x].InUse) {
			pTrunk->call[x].CRV     = iCRV;
			pTrunk->call[x].BChan   = 255;
			pTrunk->call[x].State   = 0;	/* null state - idle */
			pTrunk->call[x].TimerID = 0;	/* no timer running */
			pTrunk->call[x].Timer   = 0;
			pTrunk->call[x].InUse   = 1;	/* mark as used */
			*callIndex = x;
			return Q931E_NO_ERROR;
		}
	}
	return Q931E_TOMANYCALLS;
}

/*****************************************************************************

  Function:	 Q931GetCallState

  Description:  Look up CRV and return current call state. A non existing
				CRV is the same as state zero (0).

  Parameters:   pTrunk  [IN]	Trunk number.
				iCRV	[IN]	CRV

  Return Value: Call State.

*****************************************************************************/
L3INT Q931GetCallState(Q931_TrunkInfo_t *pTrunk, L3INT iCRV)
{
	L3INT x;
	for (x = 0; x < Q931MAXCALLPERTRUNK; x++) {
		if (pTrunk->call[x].InUse) {
			if (pTrunk->call[x].CRV == iCRV) {
				return pTrunk->call[x].State;
			}
		}
	}
	return 0; /* assume state zero for non existing CRV's */
}

/**
 * Q931StartTimer
 * \brief	Start a timer
 * \param	pTrunk		Q.931 trunk
 * \param	callindex	Index of the call
 * \param	iTimerID	ID of timer
 * \return	always 0
 */
L3INT Q931StartTimer(Q931_TrunkInfo_t *pTrunk, L3INT callIndex, L3USHORT iTimerID)
{
#if 0
	L3ULONG duration = Q931Timer[pTrunk->Dialect][iTimerID];

	if (duration) {
		pTrunk->call[callIndex].Timer   = Q931GetTime() + duration;
		pTrunk->call[callIndex].TimerID = iTimerID;
	}
#endif
	return 0;
}

/**
 * Q931StopTimer
 * \brief	Stop a timer
 * \param	pTrunk		Q.931 trunk
 * \param	callindex	Index of the call
 * \param	iTimerID	ID of timer
 * \return	always 0
 */
L3INT Q931StopTimer(Q931_TrunkInfo_t *pTrunk, L3INT callindex, L3USHORT iTimerID)
{
	if (pTrunk->call[callindex].TimerID == iTimerID)
		pTrunk->call[callindex].TimerID = 0;

	return 0;
}

L3INT Q931SetState(Q931_TrunkInfo_t *pTrunk, L3INT callIndex, L3INT iState)
{
	pTrunk->call[callIndex].State = iState;

	return 0;
}

L3ULONG Q931GetTime()
{
	L3ULONG tNow = 0;
	static L3ULONG tLast = 0;

	if (Q931GetTimeProc != NULL) {
		tNow = Q931GetTimeProc();
		if (tNow < tLast) {	/* wrapped */
			/* TODO */
		}
		tLast = tNow;
	}
	return tNow;
}

void Q931SetGetTimeCB(L3ULONG (*callback)(void))
{
	Q931GetTimeProc = callback;
}

L3INT Q931FindCRV(Q931_TrunkInfo_t *pTrunk, L3INT crv, L3INT *callindex)
{
	L3INT x;
	for (x = 0; x < Q931MAXCALLPERTRUNK; x++) {
		if (pTrunk->call[x].InUse) {
			if (pTrunk->call[x].CRV == crv) {
				*callindex = x;
				return Q931E_NO_ERROR;
			}
		}
	}
	return Q931E_INVALID_CRV;
}


void Q931AddDialect(L3UCHAR i, void (*callback)(L3UCHAR iD ))
{
	if (i < Q931MAXDLCT) {
		Q931CreateDialectCB[i] = callback;
	}
}

/*****************************************************************************
  Function:	 Q931AddStateEntry

  Description:  Find an empty entry in the dialects state table and add this
				entry.
*****************************************************************************/
void Q931AddStateEntry(L3UCHAR iD, L3INT iState,    L3INT iMes,    L3UCHAR cDir)
{
	int x;
	for (x = 0; x < Q931MAXSTATE; x++) {
		if (Q931st[x].Message == 0) {
			Q931st[x].State     = iState;
			Q931st[x].Message   = iMes;
			Q931st[x].Direction = cDir;
			/* TODO Sort table and use bsearch */
			return;
		}
	}
}

/*****************************************************************************
  Function:	 Q931IsEventLegal

  Description:  Check state table for matching criteria to indicate if this
				Message is legal in this state or not.

  Note:		 Someone write a bsearch or invent something smart here
				please - sequential is ok for now.
*****************************************************************************/
L3BOOL Q931IsEventLegal(L3UCHAR iD, L3INT iState, L3INT iMes, L3UCHAR cDir)
{
	int x;
	/* TODO Sort table and use bsearch */
	for (x = 0; x < Q931MAXSTATE; x++) {
		if (Q931st[x].State == iState && Q931st[x].Message == iMes &&
		    Q931st[x].Direction == cDir) {
			return L3TRUE;
		}
	}
	return L3FALSE;
}

/*****************************************************************************
  Function:	 q931_error_to_name()

  Description:  Check state table for matching criteria to indicate if this
				Message is legal in this state or not.

  Note:		 Someone write a bsearch or invent something smart here
				please - sequential is ok for now.
*****************************************************************************/
static const char *q931_error_names[] = {
	"Q931E_NO_ERROR",			/* 0 */

	"Q931E_UNKNOWN_MESSAGE",		/* -3001 */
	"Q931E_ILLEGAL_IE",			/* -3002 */
	"Q931E_UNKNOWN_IE",			/* -3003 */
	"Q931E_BEARERCAP",			/* -3004 */
	"Q931E_HLCOMP",				/* -3005 */
	"Q931E_LLCOMP",				/* -3006 */
	"Q931E_INTERNAL",			/* -3007 */
	"Q931E_MISSING_CB",			/* -3008 */
	"Q931E_UNEXPECTED_MESSAGE",		/* -3009 */
	"Q931E_ILLEGAL_MESSAGE",		/* -3010 */
	"Q931E_TOMANYCALLS",			/* -3011 */
	"Q931E_INVALID_CRV",			/* -3012 */
	"Q931E_CALLID",				/* -3013 */
	"Q931E_CALLSTATE",			/* -3014 */
	"Q931E_CALLEDSUB",			/* -3015 */
	"Q931E_CALLEDNUM",			/* -3016 */
	"Q931E_CALLINGNUM",			/* -3017 */
	"Q931E_CALLINGSUB",			/* -3018 */
	"Q931E_CAUSE",				/* -3019 */
	"Q931E_CHANID",				/* -3020 */
	"Q931E_DATETIME",			/* -3021 */
	"Q931E_DISPLAY",			/* -3022 */
	"Q931E_KEYPADFAC",			/* -3023 */
	"Q931E_NETFAC",				/* -3024 */
	"Q931E_NOTIFIND",			/* -3025 */
	"Q931E_PROGIND",			/* -3026 */
	"Q931E_RESTARTIND",			/* -3027 */
	"Q931E_SEGMENT",			/* -3028 */
	"Q931E_SIGNAL",				/* -3029 */
	"Q931E_GENERIC_DIGITS"			/* -3030 */

};

#define Q931_MAX_ERROR 30

const char *q931_error_to_name(q931_error_t error)
{
	int index = 0;
	if ((int)error < 0) {
		index = (((int)error * -1) -3000);
	}
	if (index < 0 || index > Q931_MAX_ERROR) {
		return "";
	}
	return q931_error_names[index];
}
/*
 * Logging
 */
#include <stdarg.h>

L3INT Q931Log(Q931_TrunkInfo_t *trunk, Q931LogLevel_t level, const char *fmt, ...)
{
	char buf[Q931_LOGBUFSIZE];
	L3INT len;
	va_list ap;

	if (!trunk->Q931LogCBProc)
		return 0;

	if (trunk->loglevel < level)
		return 0;

	va_start(ap, fmt);

	len = vsnprintf(buf, sizeof(buf)-1, fmt, ap);
	if (len <= 0) {
		/* TODO: error handling */
		return -1;
	}
	if (len >= sizeof(buf))
		len = sizeof(buf) - 1;

	buf[len] = '\0';

	va_end(ap);

	return trunk->Q931LogCBProc(trunk->PrivateDataLog, level, buf, len);
}

/**
 * Q921SetLogCB
 * \brief	Set Logging callback function and private data
 */
void Q931SetLogCB(Q931_TrunkInfo_t *trunk, Q931LogCB_t func, void *priv)
{
	trunk->Q931LogCBProc  = func;
	trunk->PrivateDataLog = priv;
}

/**
 * Q921SetLogLevel
 * \brief	Set Loglevel
 */
void Q931SetLogLevel(Q931_TrunkInfo_t *trunk, Q931LogLevel_t level)
{
    if(!trunk)
        return;

    if (level < Q931_LOG_NONE) {
        level = Q931_LOG_NONE;
    } else if (level > Q931_LOG_DEBUG) {
        level = Q931_LOG_DEBUG;
    }

	trunk->loglevel = level;
}

/**
 * Q931TimeoutDummy
 * \brief	Dummy handler for timeouts
 * \param	pTrunk		Q.931 trunk
 * \param	callIndex	Index of call
 */
L3INT Q931TimeoutDummy(Q931_TrunkInfo_t *pTrunk, L3INT callIndex)
{
	Q931Log(pTrunk, Q931_LOG_DEBUG, "Timer %d of call %d (CRV: %d) timed out\n", pTrunk->call[callIndex].TimerID, callIndex, pTrunk->call[callIndex].CRV);

	return 0;
}
