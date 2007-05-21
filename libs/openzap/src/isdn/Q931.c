/*****************************************************************************

  FileName:     Q931.c

  Contents:     Implementation of Q.931 stack main interface functions. 
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

/*****************************************************************************

  Dialect function pointers tables.

  The following function pointer arrays define pack/unpack functions and 
  processing furnctions for the different Q.931 based dialects.

  The arrays are initialized with pointers to dummy functions and later
  overrided with pointers to actual functions as new dialects are added.

  The initial Q.931 will as an example define 2 dielects as it treats User
  and Network mode as separate ISDN dialects.

  The API messages Q931AddProc, Q931AddMes, Q931AddIE are used to initialize
  these table entries during system inititialization of a stack.

*****************************************************************************/
L3INT (*Q931Proc  [Q931MAXDLCT][Q931MAXMES])	(Q931_TrunkInfo *pTrunk, L3UCHAR *,L3INT);

L3INT (*Q931Umes  [Q931MAXDLCT][Q931MAXMES])	(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT IOff, L3INT Size);
L3INT (*Q931Pmes  [Q931MAXDLCT][Q931MAXMES])	(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);

L3INT (*Q931Uie   [Q931MAXDLCT][Q931MAXIE])		(Q931_TrunkInfo *pTrunk, ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT (*Q931Pie   [Q931MAXDLCT][Q931MAXIE])		(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);

void  (*Q931CreateDialectCB[Q931MAXDLCT])       (L3UCHAR iDialect)=
{
	NULL,
	NULL
};

Q931State Q931st[Q931MAXSTATE];

/*****************************************************************************

  Core system tables and variables.

*****************************************************************************/

L3INT Q931L4HeaderSpace={0};        /* header space to be ignoder/inserted  */
                                    /* at head of each message.             */

L3INT Q931L2HeaderSpace = {4};      /* Q921 header space, sapi, tei etc     */

/*****************************************************************************

  Main interface callback functions. 

*****************************************************************************/

L3INT (*Q931Tx34Proc)(Q931_TrunkInfo *pTrunk, L3UCHAR *,L3INT);
									/* callback for messages to be send to  */
                                    /* layer 4.                             */

L3INT (*Q931Tx32Proc)(Q931_TrunkInfo *pTrunk,L3UCHAR *,L3INT);
									/* callback ptr for messages to be send */
                                    /* to layer 2.                          */

L3INT (*Q931ErrorProc)(Q931_TrunkInfo *pTrunk, L3INT,L3INT,L3INT); 
									/* callback for error messages.         */

L3ULONG (*Q931GetTimeProc) ()=NULL; /* callback for func reading time in ms */

/*****************************************************************************

  Function:     Q931SetL4HeaderSpace

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

  Function:     Q931SetL2HeaderSpace

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

  Function:     Q931

  Description:  Dummy function for message processing.

*****************************************************************************/
L3INT Q931ProcDummy(Q931_TrunkInfo *pTrunk, L3UCHAR * b,L3INT c)
{
    return Q931E_INTERNAL;
}

/*****************************************************************************

  Function:     Q931

  Description:  Dummy function for message processing

*****************************************************************************/
L3INT Q931UmesDummy(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT IOff, L3INT Size)
{
    return Q931E_UNKNOWN_MESSAGE;
}

/*****************************************************************************

  Function:     Q931

  Description:  Dummy function for message processing

*****************************************************************************/
L3INT Q931UieDummy(Q931_TrunkInfo *pTrunk,ie *pIE,L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)
{
    return Q931E_UNKNOWN_IE;
}

/*****************************************************************************

  Function:     Q931

  Description:  Dummy function for message processing

*****************************************************************************/
L3INT Q931PmesDummy(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
    return Q931E_UNKNOWN_MESSAGE;
}

/*****************************************************************************

  Function:     Q931PieDummy

  Description:  Dummy function for message processing

*****************************************************************************/
L3INT Q931PieDummy(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet)
{
    return Q931E_UNKNOWN_IE;
}

/*****************************************************************************

  Function:     Q931TxDummy

  Description:  Dummy function for message processing

*****************************************************************************/
L3INT Q931TxDummy(Q931_TrunkInfo *pTrunk, L3UCHAR * b, L3INT n)
{
    return Q931E_MISSING_CB;
}

/*****************************************************************************

  Function:     Q931ErrorDummy

  Description:  Dummy function for error processing

*****************************************************************************/
L3INT Q931ErrorDummy(Q931_TrunkInfo *pTrunk,L3INT a, L3INT b, L3INT c)
{
	return 0;
}

/*****************************************************************************

  Function:     Q931Initialize

  Description:  This function Initialize the stack. 
  
                Will set up the trunk array, channel
                arrays and initialize Q931 function arrays before it finally
                set up EuroISDN processing with User as diealect 0 and 
                Network as dialect 1.

  Note:         Initialization of other stacks should be inserted after
                the initialization of EuroISDN.

*****************************************************************************/
void Q931Initialize()
{
    L3INT x,y;

	/* Secure the callbacks to default procs */
    Q931Tx34Proc = Q931TxDummy;
    Q931Tx32Proc = Q931TxDummy;
    Q931ErrorProc = Q931ErrorDummy;

	/* The user will only add the message handlers and IE handlers he need, */
	/* so we need to initialize every single entry to a default function    */
	/* that will throw an appropriate error if they are ever called.        */
    for(x=0;x< Q931MAXDLCT;x++)
    {
        for(y=0;y<Q931MAXMES;y++)
        {
            Q931Proc[x][y] = Q931ProcDummy;
            Q931Umes[x][y] = Q931UmesDummy;
            Q931Pmes[x][y] = Q931PmesDummy;
        }
        for(y=0;y<Q931MAXIE;y++)
        {
            Q931Pie[x][y] = Q931PieDummy;
            Q931Uie[x][y] = Q931UieDummy;
        }
    }

	if(Q931CreateDialectCB[0] == NULL)
		Q931AddDialect(0, Q931CreateTE);

	if(Q931CreateDialectCB[1] == NULL)
		Q931AddDialect(1, Q931CreateNT);

	/* The last step we do is to call the callbacks to create the dialects  */
	for(x=0; x< Q931MAXDLCT; x++)
	{
		if(Q931CreateDialectCB[x] != NULL)
		{
			Q931CreateDialectCB[x]((L3UCHAR)x);
		}
	}
}

/*****************************************************************************

  Function:     Q931TimeTick

  Description:  Called periodically from an external source to allow the 
                stack to process and maintain it's own timers.

  Parameters:   ms[IN]        Milliseconds since last call.

  Return Value: none

*****************************************************************************/
void Q931TimeTick(L3ULONG ms)
{
    ms=ms; /* avoid warning for now. */

	/*  TODO: Loop through all active calls, check timers and call timour procs
	 *  if timers are expired.
	 *  Implement an function array so each dialect can deal with their own
	 *  timeouts.
	 */
}

/*****************************************************************************

  Function:     Q931Rx23

  Description:  Receive message from layer 2 (LAPD). Receiving a message 
                is always done in 2 steps. First the message must be 
                interpreted and translated to a static struct. Secondly
                the message is processed and responded to.

				The Q.931 message contains a static header that is 
				interpreated in his function. The rest is interpreted
				in a sub function according to mestype.

  Parameters:   pTrunk	[IN]	Ptr to trunk info.
				buf		[IN]	Ptr to buffer containing message.
				Size	[IN]	Size of message.

  Return Value: Error Code. 0 = No Error, < 0 :error, > 0 : Warning
				see q931errors.h for details.

*****************************************************************************/
L3INT Q931Rx23(Q931_TrunkInfo *pTrunk, L3UCHAR * buf, L3INT Size)
{
    L3UCHAR *Mes = &buf[Q931L2HeaderSpace];
	L3INT RetCode = Q931E_NO_ERROR;

    Q931mes_Alerting * m = (Q931mes_Alerting*)Mes;
    L3INT ISize;
    L3INT IOff = 0;

    /* Protocol Discriminator */
    m->ProtDisc = Mes[IOff++];

    /* CRV */
    m->CRV = Q931Uie_CRV(pTrunk, Mes,m->buf, &IOff, &ISize);

    /* Message Type */
    m->MesType = Mes[IOff++];

    /* Call table proc to unpack codec message */
	RetCode = Q931Umes[pTrunk->Dialect][m->MesType](pTrunk, Mes, pTrunk->L3Buf,Q931L4HeaderSpace,Size- Q931L4HeaderSpace);
	if(RetCode >= Q931E_NO_ERROR)
	{
		RetCode=Q931Proc[pTrunk->Dialect][m->MesType](pTrunk, pTrunk->L3Buf, 2);
	}

    return RetCode;
}

/*****************************************************************************

  Function:		Q931Tx34

  Description:	Called from the stac to send a message to layer 4.

  Parameters:	Mes[IN]		Ptr to message buffer.
				Size[IN]	Message size in bytes.

  Return Value: Error Code. 0 = No Error, < 0 :error, > 0 : Warning
				see q931errors.h for details.

*****************************************************************************/
L3INT Q931Tx34(Q931_TrunkInfo *pTrunk, L3UCHAR * Mes, L3INT Size)
{
    return Q931Tx34Proc(pTrunk, Mes, Size);
}

/*****************************************************************************

  Function:     Q931Rx43

  Description:  Receive message from Layer 4 (application).

  Parameters:   pTrunk[IN]  Trunk #.
                buf[IN]     Message Pointer.
                Size[IN]    Message size in bytes.

  Return Value: Error Code. 0 = No Error, < 0 :error, > 0 : Warning
				see q931errors.h for details.

*****************************************************************************/
L3INT Q931Rx43(Q931_TrunkInfo *pTrunk,L3UCHAR * buf, L3INT Size)
{
    Q931mes_Header *ptr = (Q931mes_Header*)&buf[Q931L4HeaderSpace];
	L3INT RetCode = Q931E_NO_ERROR;

	RetCode=Q931Proc[pTrunk->Dialect][ptr->MesType](pTrunk,buf,4);

    return RetCode;
}

/*****************************************************************************

  Function:		Q931Tx32

  Description:	Called from the stack to send a message to L2. The input is 
                always a non-packed message so it will first make a proper
                call to create a packed message before it transmits that
                message to layer 2.

  Parameters:	pTrunk[IN]  Trunk #  
                buf[IN]		Ptr to message buffer.
				Size[IN]	Message size in bytes.

  Return Value: Error Code. 0 = No Error, < 0 :error, > 0 : Warning
				see q931errors.h for details.

*****************************************************************************/
L3INT Q931Tx32(Q931_TrunkInfo *pTrunk, L3UCHAR * Mes, L3INT Size)
{
    L3INT     OSize;
    Q931mes_Alerting *ptr = (Q931mes_Alerting*)Mes;
	L3INT RetCode = Q931E_NO_ERROR;
    L3INT iDialect = pTrunk->Dialect;

    /* Call pack function through table. */
    RetCode = Q931Pmes[iDialect][ptr->MesType](pTrunk,Mes,Size,&pTrunk->L2Buf[Q931L2HeaderSpace], &OSize);
	if(RetCode >= Q931E_NO_ERROR)
	{
        RetCode = Q931Tx32Proc(pTrunk, pTrunk->L2Buf, Size);
    }

    return RetCode;
}


/*****************************************************************************

  Function:		Q931SetError

  Description:	Called from the stack to indicate an error. 

  Parameters:	ErrID       ID of ie or message causing error.
				ErrPar1     Error parameter 1
                ErrPar2     Error parameter 2.


*****************************************************************************/
void Q931SetError(Q931_TrunkInfo *pTrunk,L3INT ErrID, L3INT ErrPar1, L3INT ErrPar2)
{
    Q931ErrorProc(pTrunk,ErrID, ErrPar1, ErrPar2);
}

void Q931SetTx34CB(L3INT (*Q931Tx34Par)(Q931_TrunkInfo *pTrunk,L3UCHAR * Mes, L3INT Size))
{
    Q931Tx34Proc = Q931Tx34Par;
}

void Q931SetTx32CB(L3INT (*Q931Tx32Par)(Q931_TrunkInfo *pTrunk,L3UCHAR * Mes, L3INT Size))
{
    Q931Tx32Proc = Q931Tx32Par;
}

void Q931SetErrorCB(L3INT (*Q931ErrorPar)(Q931_TrunkInfo *pTrunk,L3INT,L3INT,L3INT))
{
    Q931ErrorProc = Q931ErrorPar;
}

/*****************************************************************************

  Function:		Q931CreateCRV

  Description:	Create a CRV entry and return it's index. The function will 
				locate a free entry in the call tables allocate it and 
				allocate a unique CRV value attached to it.

  Parameters:	pTrunk		[IN]	Trunk number
				callindex	[OUT]	return call table index.

  Return Value: Error Code. 0 = No Error, < 0 :error, > 0 : Warning
				see q931errors.h for details.

*****************************************************************************/
L3INT	Q931CreateCRV(Q931_TrunkInfo *pTrunk, L3INT * callIndex)
{
	L3INT CRV = Q931GetUniqueCRV(pTrunk);

	return Q931AllocateCRV(pTrunk, CRV, callIndex);
}

/*****************************************************************************

  Function:		Q931AllocateCRV

  Description:	Allocate a call table entry and assigns the given CRV value
				to it.

  Parameters:	pTrunk		[IN]	Trunk number
				iCRV		[IN]	Call Reference Value.
				callindex	[OUT]	return call table index.

  Return Value: Error Code. 0 = No Error, < 0 :error, > 0 : Warning
				see q931errors.h for details.

*****************************************************************************/
L3INT	Q931AllocateCRV(Q931_TrunkInfo *pTrunk, L3INT iCRV, L3INT * callIndex)
{
    L3INT x;
	for(x=0; x < Q931MAXCALLPERTRUNK; x++)
	{
		if(!pTrunk->call[x].InUse)
		{
			pTrunk->call[x].CRV	= iCRV;
			pTrunk->call[x].BChan = 255;
			pTrunk->call[x].State = 0;		/* null state - idle */
			pTrunk->call[x].TimerID = 0;	/* no timer running */
			pTrunk->call[x].Timer = 0;
			pTrunk->call[x].InUse = 1;		/* mark as used */
            *callIndex = x;
            return Q931E_NO_ERROR;
		}
	}
    return Q931E_TOMANYCALLS;
}

/*****************************************************************************

  Function:     Q931GetCallState

  Description:  Look up CRV and return current call state. A non existing
                CRV is the same as state zero (0).

  Parameters:   pTrunk	[IN]    Trunk number.
                iCRV	[IN]	CRV

  Return Value: Call State.

*****************************************************************************/
L3INT	Q931GetCallState(Q931_TrunkInfo *pTrunk, L3INT iCRV)
{
    L3INT x;
	for(x=0; x < Q931MAXCALLPERTRUNK; x++)
	{
		if(!pTrunk->call[x].InUse)
		{
            if(pTrunk->call[x].CRV == iCRV)
            {
                return pTrunk->call[x].State;
            }
        }
    }
    return 0; /* assume state zero for non existing CRV's */
}

/*****************************************************************************

  Function:     Q931StartTimer

  Description:  Start a timer.

  Parameters:   pTrunk      Trunk number
                callindex   call index.
                iTimer      timer id
*****************************************************************************/
L3INT Q931StartTimer(Q931_TrunkInfo *pTrunk, L3INT callIndex, L3USHORT iTimerID)
{
    pTrunk->call[callIndex].Timer   = Q931GetTime();  
    pTrunk->call[callIndex].TimerID = iTimerID;
    return 0;
}

L3INT Q931StopTimer(Q931_TrunkInfo *pTrunk, L3INT callindex, L3USHORT iTimerID)
{
    if(pTrunk->call[callindex].TimerID == iTimerID)
        pTrunk->call[callindex].TimerID = 0;
    return 0;
}

L3INT Q931SetState(Q931_TrunkInfo *pTrunk, L3INT callIndex, L3INT iState)
{
    pTrunk->call[callIndex].State = iState;

    return 0;
}

L3ULONG Q931GetTime()
{
    L3ULONG tNow = 0;
    static L3ULONG tLast={0};
    if(Q931GetTimeProc != NULL)
    {
        tNow = Q931GetTimeProc();
        if(tNow < tLast)    /* wrapped */
        {
			/* TODO */
        }
		tLast = tNow;
    }
    return tNow;
}

void Q931SetGetTimeCB(L3ULONG (*callback)())
{
    Q931GetTimeProc = callback;
}

L3INT Q931FindCRV(Q931_TrunkInfo *pTrunk, L3INT crv, L3INT *callindex)
{
    L3INT x;
	for(x=0; x < Q931MAXCALLPERTRUNK; x++)
	{
		if(!pTrunk->call[x].InUse)
		{
            if(pTrunk->call[x].CRV == crv)
            {
                *callindex = x;
                return Q931E_NO_ERROR;
            }
        }
    }
    return Q931E_INVALID_CRV;
}


void Q931AddDialect(L3UCHAR i, void (*callback)(L3UCHAR iD ))
{
	if(i < Q931MAXDLCT)
	{
		Q931CreateDialectCB[i] = callback;
	}
}

/*****************************************************************************
  Function:		Q931AddStateEntry

  Description:	Find an empty entry in the dialects state table and add this
				entry.
*****************************************************************************/
void Q931AddStateEntry(L3UCHAR iD, L3INT iState,	L3INT iMes,	L3UCHAR cDir)
{
	int x;
	for(x=0; x < Q931MAXSTATE; x++)
	{
		if(Q931st[x].Message == 0)
		{
			Q931st[x].State = iState;
			Q931st[x].Message = iMes;
			Q931st[x].Direction = cDir;
			/* TODO Sort table and use bsearch */
			return;
		}
	}
}

/*****************************************************************************
  Function:		Q931IsEventLegal

  Description:	Check state table for matching criteria to indicate if this
				Message is legal in this state or not.

  Note:			Someone write a bsearch or invent something smart here
				please - sequensial is ok for now.
*****************************************************************************/
L3BOOL Q931IsEventLegal(L3UCHAR iD, L3INT iState, L3INT iMes, L3UCHAR cDir)
{
	int x;
	/* TODO Sort table and use bsearch */
	for(x=0; x < Q931MAXSTATE; x++)
	{
		if(		Q931st[x].State == iState
			&&  Q931st[x].Message == iMes
			&&  Q931st[x].Direction == cDir)
		{
			return L3TRUE;
		}
	}
	return L3FALSE;	
}
