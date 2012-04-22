/*****************************************************************************

  FileName:		Q932mes.c

  Contents:		Q.932 Message Encoders/Decoders

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

  Function:	 Q932Umes_Facility

*****************************************************************************/

L3INT Q932Umes_Facility(Q931_TrunkInfo_t *pTrunk,L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;

	/* TODO */

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Facility

*****************************************************************************/
L3INT Q932Pmes_Facility(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	L3BOOL RetCode = L3FALSE;

	NoWarning(OBuf);
	NoWarning(IBuf);

	return RetCode;
}

/*****************************************************************************

  Function:	 Q932Umes_Hold

*****************************************************************************/

L3INT Q932Umes_Hold(Q931_TrunkInfo_t *pTrunk,L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;

	/* TODO */

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Hold

*****************************************************************************/
L3INT Q932Pmes_Hold(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	L3BOOL RetCode = L3FALSE;

	NoWarning(OBuf);
	NoWarning(IBuf);

	return RetCode;
}

/*****************************************************************************

  Function:	 Q932Umes_HoldAck

*****************************************************************************/

L3INT Q932Umes_HoldAck(Q931_TrunkInfo_t *pTrunk,L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;

	/* TODO */

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_HoldAck

*****************************************************************************/
L3INT Q932Pmes_HoldAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	L3BOOL RetCode = L3FALSE;

	NoWarning(OBuf);
	NoWarning(IBuf);

	return RetCode;
}

/*****************************************************************************

  Function:	 Q932Umes_HoldReject

*****************************************************************************/

L3INT Q932Umes_HoldReject(Q931_TrunkInfo_t *pTrunk,L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;

	/* TODO */

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_HoldReject

*****************************************************************************/
L3INT Q932Pmes_HoldReject(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	L3BOOL RetCode = L3FALSE;

	NoWarning(OBuf);
	NoWarning(IBuf);

	return RetCode;
}

/*****************************************************************************

  Function:	 Q932Umes_Register

*****************************************************************************/

L3INT Q932Umes_Register(Q931_TrunkInfo_t *pTrunk,L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;

	/* TODO */

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Register

*****************************************************************************/
L3INT Q932Pmes_Register(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	L3BOOL RetCode = L3FALSE;

	NoWarning(OBuf);
	NoWarning(IBuf);

	return RetCode;
}

/*****************************************************************************

  Function:	 Q932Umes_Retrieve

*****************************************************************************/

L3INT Q932Umes_Retrieve(Q931_TrunkInfo_t *pTrunk,L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;

	/* TODO */

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_Retrieve

*****************************************************************************/
L3INT Q932Pmes_Retrieve(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	L3BOOL RetCode = L3FALSE;

	NoWarning(OBuf);
	NoWarning(IBuf);

	return RetCode;
}

/*****************************************************************************

  Function:	 Q932Umes_RetrieveAck

*****************************************************************************/

L3INT Q932Umes_RetrieveAck(Q931_TrunkInfo_t *pTrunk,L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;

	/* TODO */

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_RetrieveAck

*****************************************************************************/
L3INT Q932Pmes_RetrieveAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	L3BOOL RetCode = L3FALSE;

	NoWarning(OBuf);
	NoWarning(IBuf);

	return RetCode;
}

/*****************************************************************************

  Function:	 Q932Umes_RetrieveReject

*****************************************************************************/

L3INT Q932Umes_RetrieveReject(Q931_TrunkInfo_t *pTrunk,L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size)
{
	L3INT OOff = 0;

	/* TODO */

	mes->Size = sizeof(Q931mes_Generic) - 1 + OOff;
	return Q931E_NO_ERROR;
}

/*****************************************************************************

  Function:	 Q931Pmes_RetrieveReject

*****************************************************************************/
L3INT Q932Pmes_RetrieveReject(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize)
{
	L3BOOL RetCode = L3FALSE;

	NoWarning(OBuf);
	NoWarning(IBuf);

	return RetCode;
}
