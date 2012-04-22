/******************************************************************************

  FileName:         national.h

  Contents:         Header and definition for the National ISDN dialect. The 
										header contents the following parts:
                    - Definition of codes
                    - Definition of information elements (nationalie_).
                    - Definition of messages (nationalmes_).
                    - Function prototypes.

  Description:		The National ISDN dialect here covers ????

  Related Files:	national.h			National ISDN Definitions
			nationalie.c			National ISDN IE encoders/coders
			nationalStateTE.c		National ISDN TE State Engine
			nationalStateNT.c		National ISDN NT State Engine

  License/Copyright:

  Copyright (c) 2007, Jan Vidar Berger, Case Labs, Ltd. All rights reserved.
  email:janvb@caselaboratories.com  

  Copyright (c) 2007, Michael Jerris. All rights reserved.
  email:mike@jerris.com  

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
******************************************************************************/

#ifndef _DMS_NL
#define _DMS_NL

#include "Q931.h"

/*****************************************************************************

  Q.931 Message codes
  Only National specific message and ie types 
  here the rest are inherited from national.h
  
*****************************************************************************/


/*****************************************************************************

  Q.931 Message Pack/Unpack functions. Implemented in nationalmes.c

*****************************************************************************/
L3INT DMSUmes_Setup(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT IOff, L3INT Size);
L3INT DMSPmes_Setup(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT DMSUmes_0x07(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size);
L3INT DMSPmes_0x07(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT DMSUmes_0x0f(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size);
L3INT DMSPmes_0x0f(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);

/*****************************************************************************

  Q.931 Process Function Prototyping. Implemented in nationalStateTE.c

*****************************************************************************/

L3INT DMSProc0x0fTE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom);
L3INT DMSProc0x07TE(Q931_TrunkInfo_t *pTrunk, L3UCHAR * buf, L3INT iFrom);

void DMSCreateTE(L3UCHAR i);
void DMSCreateNT(L3UCHAR i);

#endif /* _DMS_NL */
